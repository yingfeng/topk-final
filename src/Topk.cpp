#include "Topk.h"

Topk::Topk(char * file,size_t *file_sizes, int num_files) {
    // Loading the Text
    char *text=NULL;
    cout << "loading file" << endl;
    if(loadText(file, &text, &this->length))
        return;
    cout << "file length:" << this->length << endl;
    cout << "constructing suffix Tree" << endl;
    this->cst = new SuffixTreeY(text, this->length, NAIVE, CN_NPR, 32);
    cout << "Done!" << endl;
    cout << "Constructing Suffix Array" << endl;
    this->ticsa = new TextIndexCSA((uchar *)text, (ulong)this->length, NULL);
    cout << "Done!" << endl;
    delete[] text;

    // Suffix Tree Handler provides several functions
    this->stp = new SuffixTreeHandler(cst,length);
   
    // Retrieve all Nodes in pre-Order
    // preorder_vector[i] -> [l,r]
    // vector<pair<uint,uint> >  preorder_vector;
    pair<BitString *,uint> tree_parenthesis = this->stp->generateBitmap(5*length,this->preorder_vector);
    map<pair<uint,uint>,uint>  nodes;

    // create an inverse map, to index according to [l,r]
    // [x,y] -> i
    cout << "preorder_vector size = " << this->preorder_vector.size() << endl;
    for (int i = 0 ; i < preorder_vector.size();i++) {
        this->nodes_preorder[make_pair(this->preorder_vector[i].first,this->preorder_vector[i].second)] = i;
    }
    // this->nodes_preorder = nodes;
    this->number_of_nodes = preorder_vector.size();
    this->t = new tree_ff(tree_parenthesis.first->getData(),tree_parenthesis.second,0);
    delete tree_parenthesis.first;
    this->d = buildBs(file_sizes,length,num_files);

    this->da = new DocumentArray(cst,this->d,preorder_vector,ticsa); 
    this->ll = new LinkList(2*length);
    int *CARRAY = this->da->createCArray();
    this->CRMQ = new RMQ(CARRAY,length);
    cout << "Filling linklist..." << endl;
    this->fillLinkList(num_files);
    cout << " DONE ! " << endl;
    cout << "generating sequence" << endl;
    this->generateSequence();
    cout << "DONE!" << endl;
}

void Topk::fillLinkList(uint num_files) {
    for (uint i = 1 ; i <= num_files;i++) {
        size_t pos = 0;
        // Retrieve the positions of document i-1 in position pos+1 and pos+2.
        pair<size_t, size_t> pairc = da->selectDocument(i-1,pos+1);
        while(pairc.first <= this->length+1 && pairc.second <= this->length+1) {
            size_t vl_lca,vr_lca;
            // obtain the LCA of both of them
            this->cst->LCA(pairc.first,pairc.first,pairc.second,pairc.second,&vl_lca,&vr_lca);
  //          uint lca = this->t->Lca(node1,node2);
            // if something goes out of place...
            if (vl_lca == (size_t)-1 || vr_lca == (size_t)-1)
                break;
            // insert in the linked list:
            // the lca node number
            // the document that corresponds
            // the frequency for that node and document.
            this->ll->insert(nodes_preorder[make_pair(vl_lca,vr_lca)],i-1,da->countRange(i-1,vl_lca,vr_lca));
            // cout << "inserting " << nodes_preorder[make_pair(vl_lca,vr_lca)] << endl;
            pos++;
            pairc = this->da->selectDocument(i-1,pos+1);                
            if (pos >= this->length)
                break;
        }
    }
}

void Topk::generateSequence() {
	cout << "Constructing Sequences" << endl;
        this->max_freq = 0;
        size_t sum = 0;
        vector<uint> frequencies;
        vector<uint> depth_sequence;
        size_t pointersize = 0;
        size_t bitmap_size = 0;
        // a 1 is set every time a node is being examinated. 
        // all 0's corresponds to different documents for the same node.
        BitString *bsmap = new BitString(this->number_of_nodes);
        // if is a leaf, mark it here
        // map leaf is to map the cst indexing to the pre-order traversal.
        BitString *map_leaf = new BitString(this->number_of_nodes);
        size_t internal_nodes = 0;
        size_t vl_p,vr_p;

        for (uint i = 0; i < this->number_of_nodes;i++) {
    	    if (i % 1000000 == 0) {
        		cout << i*1.0/this->number_of_nodes*1.0 << "(" << i << "/" << this->number_of_nodes << ")" <<  endl;
            }
        
            pair<uint,uint> aux_node = this->preorder_vector[i];
            if (aux_node.first == aux_node.second) {
                map_leaf->setBit(i);
                continue;
            }

            bsmap->setBit(internal_nodes+depth_sequence.size());
            internal_nodes++;

            size_t k = 0;
            // get the i-th node, the k-th document.
            // the document is aux.first
            // the frequency of that node, for that document is aux.second
            pair<uint,uint> aux = this->ll->getValue(i,k);
            if (this->ll->getSize(i) == 0) {
                continue;
            }

            size_t vl,vr;
            vl = aux_node.first;
            vr = aux_node.second;
            while(aux.second != (uint)-1) {
                cst->Parent(vl,vr,&vl_p,&vr_p);
                // there is NO parent with a higher frequency...
                if (vl_p == (size_t)-1 || vr_p == (size_t)-1) {             
                    break;
                }
                // get the document_id and frequency of the parent (aux2)
                size_t l = 0;
                pair<uint,uint> aux2 = ll->getValue(nodes_preorder[make_pair(vl_p,vr_p)],l);
                size_t p1r = 0;
                size_t p1l = 0;
                // SI EL PADRE NO TIENE NADA!
                // BUSCO ALGUN PADRE QUE TENGA ALGO!
                while(aux2.first == (uint)-1 && i != 0) {
                    l = 0;
                    this->cst->Parent(vl_p, vr_p, &p1l,  &p1r);
                    aux2 = ll->getValue(nodes_preorder[make_pair(p1l, p1r)], l);
                    vl_p = p1l;
                    vr_p = p1r;
                }
                // BUSCO EL DOCUMENT O
                while (aux2.first != aux.first) { 
                    l++;
                    aux2 = ll->getValue(nodes_preorder[make_pair(vl_p, vr_p)], l);
                    if (aux2.first == (uint)-1) { 
                        // NO ENCONTRE EL DOCUMENTO ACA
                        break;
                    }
                    // if (aux2.first > aux.first) {
                    //     break;
                    // }
                }
                // while (aux2.second < aux.second && aux.first == aux2.first) {
                //     // cout << "@ Searching greater ferq = " << internal_nodes << "with document = " << aux.second << "vs = " << aux2.second << " l = " <<  l << endl; 
                //     aux2 = ll->getValue(nodes_preorder[make_pair(vl_p, vr_p)], l);
                //     l++;
                // }
                if (aux2.first == aux.first && aux2.second > aux.second) {
                    size_t tdepth = this->cst->TDepth(vl_p,vr_p); // calculate the tree depth
                    depth_sequence.push_back(tdepth); // add the Tree Depth (0 is reserved for the dummy ones)
                    this->documents.push_back(aux.first); // add the document 
                    frequencies.push_back(aux.second); // add the frequency
                    if (aux.second > max_freq)
                        this->max_freq = aux.second;
                    vl = aux_node.first;
                    vr = aux_node.second;
                    uint old_doc = aux.first;
                    while (old_doc == aux.first) {
                        k++; // get the next document,freq of the node being examinated
                        aux = this->ll->getValue(i,k); 
                    }
                } else {
                    vl = vl_p; // set the initial values, to get the parent of the parent and so on...
                    vr = vr_p;
                }
            } // end while parent
        } // end for next i

    cout << "DONE WITH SEQUENCE! " << endl;
    uint *bsmap_data =  bsmap->getData();  
    uint *leaf_data = map_leaf->getData();
    //  for (int i = 0 ; i < this->number_of_nodes+depth_sequence.size();i++) {
    //      cout << "bsmap[" << i << "] = " << bsmap->getBit(i) << " | " << bsleaf->getBit(i) << endl;
    //  }
    // cout << "done printing!" << endl;
    // cout << "creating new bitstrings" << endl;
    // resize the bitmaps to their corresponding sizes.
    bsmap = new BitString(bsmap_data,internal_nodes+depth_sequence.size()+1);
    //bsleaf = new BitString(bsleaf_data,internal_nodes+depth_sequence.size()+1);
    map_leaf = new BitString(leaf_data,this->number_of_nodes+2);
    // cout << "setting last bit " << endl;
    bsmap->setBit(this->number_of_nodes+depth_sequence.size());
    //bsleaf->setBit(this->number_of_nodes+depth_sequence.size());
    map_leaf->setBit(this->number_of_nodes);
    // cout << "constructing bitsequence" << endl;
    this->bitsequence_map = new BitSequenceRG(*bsmap,20);
    //this->bitsequence_leaf = new BitSequenceRG(*bsleaf,20);
    this->bitmap_leaf = new BitSequenceRG(*map_leaf,20);
    delete[] bsmap_data;
    //delete[] bsleaf_data;
    delete[] leaf_data;
    // cout << "done!" << endl;
    nodes_preorder.clear();
    this->pointer_size = depth_sequence.size();
    uint *depth_sequence_array = new uint[depth_sequence.size()];
    // cout << "hardcopying the depth sequences " << endl;
    // hard copy depth_sequence
    this->freq_array = new uint[depth_sequence.size()];
    uint *norm_weight = new uint[depth_sequence.size()];

    for (size_t i = 0 ; i < depth_sequence.size();i++) {
         depth_sequence_array[i] = depth_sequence[i];
    	 this->freq_array[i] = frequencies[i];
	    norm_weight[i] = this->max_freq - frequencies[i];
    }

    this->gd_sequence = &depth_sequence[0];
    uint *document_array = new uint[documents.size()];
    document_array = &documents[0];
    // copy(documents.begin(), documents.end(), document_array);
    // cout << "Sorting Freqs and documents..." << endl;
    // using stable sort, to match to the leafs of the wavelet tree
    assert(documents.size() == depth_sequence.size());
    sort(this->gd_sequence,this->freq_array,document_array,depth_sequence.size());
    // cout << "Done!" << endl;
    // for (int i = 0 ; i < this->pointer_size;i++ )
    // {
    //     cout << "| " << i << " | " << gd_sequence[i] << " | " <<  this->freq_array[i] << " | " << document_array[i] << endl;
    // }
    this->doc_array = new Array(document_array,this->pointer_size);
    this->freq_dacs = new factorization(this->freq_array,this->pointer_size);

    // cout << "Constructing Wavelet Tree of Depth Sequences" << endl;
    Array *A = new Array(depth_sequence_array,this->pointer_size);
    MapperNone * map = new MapperNone();
    BitSequenceBuilder * bsb = new BitSequenceBuilderRG(20);
    this->d_sequence = new WaveletTreeRMQ(*A, bsb, map,norm_weight);

    // for (int i = 0; i < A->getLength(); i++)
    //     assert(A->getField(i) == d_sequence->access(i));

  //  for (int i = 0; i < A->getLength(); i++) {
  //      cout << i << " " << ((this->max_freq - norm_weight[i]) == (this->freq_array[i])) << " " << gd_sequence[i] << " " << (this->max_freq - norm_weight[i]) << " " << (this->freq_array[i]) << endl;
  //  }
    //frequencies.clear();
   // documents.clear();
    depth_sequence.clear();
    delete[] depth_sequence_array;
    delete[] document_array;
   // delete[] gd_sequence;
    delete A;
    delete []norm_weight;
    delete this->da;
    delete this->ll;
    delete this->stp;
    delete bsmap;
    delete map_leaf;
    //delete bsleaf;
  //  cout << "Done! " << endl;
}
void Topk::documentList(uint i, uint j,uint k,map<uint,uint> &results) {
  if (i>j) return;
  if (k == 0) return;
  uint pos = this->CRMQ->query(i,j);
  uint doc = this->d->rank1(this->ticsa->getSA(pos)); 
  if (results.find(doc) != results.end()) {
    return;
  }
  else {
    results[doc] = 1;
    k--;
  } 
  documentList(i,pos-1,k,results);
  documentList(pos+1,j,k,results);

}

pair<double,double> Topk::query(uchar *q,uint size_q,uint k) {
    Timer *t1 = new Timer();

//    cout << "received query:" << q << endl;
    pair<int, int> posn = ticsa->count(q,size_q);
    if (posn.second - posn.first + 1 == 0) {
        t1->Stop();
       return make_pair(-1,t1->ElapsedTimeCPU());
    }
//     cout << "numocc = " << posn.second - posn.first + 1 << endl;
    size_t start_range = posn.first;
    size_t end_range = posn.second;
    if (end_range > length) end_range = length;
//     cout << "start_range = " << start_range << endl;
//     cout << "end_range = " << end_range << endl;
    // // if is a leaf:
    if (start_range == end_range ) {
        // uint result = this->d_array[this->bitsequence_leaf->select1(start_range)];
         clock_t end=clock();
         t1->Stop();
         return make_pair(-1,t1->ElapsedTimeCPU());
    }
    t1->Stop();

    Timer *t2 = new Timer();
    uint l1 = this->bitmap_leaf->select1(start_range);
    uint l2 = this->bitmap_leaf->select1(end_range);
    uint l11 = this->t->Preorden_Select(l1 + 1);
    uint l22 =  this->t->Preorden_Select(l2 + 1);
    uint lca = this->t->Lca(l11,l22);
    uint tdepth;
    uint p;
    uint pp;
    
    p = this->t->Preorder_Rank(lca);
    tdepth = this->t->Depth(lca);
    pp = p + this->t->Subtree_Size(lca);
    uint leaves_start = this->bitmap_leaf->rank1(p);
    uint leaves_end = this->bitmap_leaf->rank1(pp-1);
    uint new_p = p - leaves_start;
    uint new_pp = pp - leaves_end;
    size_t s_new_range = this->bitsequence_map->select1(new_p) - new_p + 1;
    size_t e_new_range = this->bitsequence_map->select1(new_pp) - new_pp;


    vector<pair<uint,uint> > v = this->d_sequence->range_call(s_new_range, e_new_range, 0, tdepth, k);
    uint new_k = v.size();
    if (new_k < k) {
        map<uint,uint> res;
        for (int i = 0 ; i < v.size();i++) {
            uint doc = this->doc_array->getField(v[i].second);
            res[doc] = 1;
        }
        documentList(s_new_range,e_new_range,k-new_k,res);
    }

    // uint doc,weight;
    // for (int i = 0 ; i < v.size();i++) {
    //     doc = this->doc_array->getField(v[i].second);
    //     weight = this->max_freq - v[i].first;
    // }
    t2->Stop();
    return make_pair(t2->ElapsedTimeCPU(),t1->ElapsedTimeCPU());

}


Topk::~Topk() {
    delete this->cst;
    delete[] this->freq_array;
    delete this->ticsa;
    delete this->d_sequence;
    //delete[] this->gd_sequence;
    delete this->t;
    //delete []this->d_array;
    delete this->freq_dacs;
//    delete this->bitsequence_leaf;
    delete this->bitsequence_map;
    delete this->d;
}


size_t Topk::getSize() {
    size_t cst_size = this->cst->getSize();
    size_t csa_size = this->ticsa->getSize();
    size_t wt_size = this->d_sequence->getSize();
    size_t freq_array_size = this->freq_dacs->getSize();
    size_t tree_size = this->t->size();
    size_t map_size = this->bitsequence_map->getSize() + this->bitmap_leaf->getSize();
    size_t documents_size = this->doc_array->getSize();
    size_t rmq_carray_size = this->CRMQ->getSize();
    size_t document_bitmap_size = this->d->getSize();
    size_t internal_nodes = this->bitsequence_map->countOnes();
    size_t total = csa_size + wt_size + freq_array_size + map_size + tree_size + documents_size + rmq_carray_size + document_bitmap_size;
    size_t total2 = csa_size + wt_size + freq_array_size + map_size + tree_size*4 + documents_size + rmq_carray_size*4 + document_bitmap_size;
    size_t leaf_nodes = this->bitmap_leaf->countOnes();

    cout << "CSA SIZE \t WT SIZE \t FREQ ARRAY \t TREE SIZE \t MAP SIZE \t DOCUMENT ARRAY \t CARRAY_RMQ \t DOC BITMAP \t  TOTAL \t TOTAL(MB) \t RATIO " << endl;
    cout << csa_size << "\t" << wt_size << "\t" << freq_array_size << "\t" << tree_size << "\t" << map_size << "\t" << documents_size << "\t" << rmq_carray_size << "\t" << document_bitmap_size << "\t" << total << "\t" << total/(1024.00*1024.00) << "\t" << (total*1.0)/(this->length*1.0) << "\t" << endl;
//    cout << "---------------------------------------------------------------";
//    cout << csa_size << "\t" << wt_size << "\t" << freq_array_size << "\t" << tree_size*4 << "\t" << map_size << "\t" << documents_size << "\t" << rmq_carray_size*4 << "\t" << document_bitmap_size << "\t" << total2 << "\t" << total2/(1024.00*1024.00) << "\t" << (total2*1.0)/(this->length*1.0) << "\t" << endl;

    cout << "Internal nodes:" << internal_nodes << endl;
    cout << "Leaf nodes:" << leaf_nodes << endl;
    cout << "Totla nodes:" << this->number_of_nodes << endl;
    cout << "Grid alphabet:" << this->d_sequence->getMaxValue() << endl;
    cout << "Grid Height:" << this->d_sequence->getHeight() << endl;
    return total;

}
