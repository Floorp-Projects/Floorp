
/**
 * This file defines the binary tree class and it
 * nsNode child class.
 *
 * This simple version stores nodes, and the
 * nodes store void* ptrs.
 * 
 * @update gess 4/11/98
 * @param  
 * @return 
 */

#include "nsBTree.h"

/**
 * default constructor
 * 
 * @update gess 4/11/98
 */
nsNode::nsNode(){
  mLeft=0;
  mRight=0;
  mParent=0;
  mColor=eBlack;
}


/**
 * Copy constructor
 * 
 * @update gess 4/11/98
 * @param  
 * @return 
 */
nsNode::nsNode(const nsNode& aNode){
  mLeft=aNode.mLeft;
  mRight=aNode.mRight;
  mParent=aNode.mParent;
  mColor=aNode.mColor;
}

/**
 * destructor
 * 
 * @update gess 4/11/98
 */
nsNode::~nsNode(){
}

/**
 * Retrive ptr to parent node
 * 
 * @update gess 4/11/98
 * @return ptr to parent node
 */
nsNode* nsNode::GetParentNode(void) const{
  return mParent;
}

/**
 * Retrieve ptr to left (less) node
 * 
 * @update gess 4/11/98
 * @return ptr to left (may be NULL)
 */
nsNode* nsNode::GetLeftNode(void) const{
  return mLeft;
}

/**
 * Retrieve ptr to right (more) node
 * 
 * @update gess 4/11/98
 * @return ptr to right node (may be NULL)
 */
nsNode* nsNode::GetRightNode(void) const{
  return mRight;
}


/**
 * 
 * 
 * @update gess 4/11/98
 * @param  
 * @return 
 */
nsNode& nsNode::operator=(const nsNode& aNode){
  if(this!=&aNode){
    mLeft=aNode.mLeft;
    mRight=aNode.mRight;
    mParent=aNode.mParent;
    mColor=aNode.mColor;
  }
  return *this;
}

/********************************************************
 * Here comes the BTREE class...
 ********************************************************/

/**
 * nsBTree constructor
 * 
 * @update gess 4/11/98
 */
nsBTree::nsBTree(){
  mRoot=0;
}


/**
 * destructor
 * 
 * @update gess 4/11/98
 */
nsBTree::~nsBTree(){
  if(mRoot){
    //walk the tree and destroy the children.
  }
}
  
/**
 * Given a node, we're supposed to add it into 
 * our tree.
 * 
 * @update gess 4/11/98
 * @param  aNode to be added to tree
 * @return ptr to added node or NULL
 */
nsNode* nsBTree::Add(nsNode& aNode){
  PRBool result=PR_TRUE;

  nsNode* node1=mRoot; //x
  nsNode* node2=0;     //y
  while(node1) {
    node2=node1;
    if(aNode<*node1)
      node1=node1->mLeft;
    else node1=node1->mRight;
  }
  aNode.mParent=node2;
  if(!node2){
    mRoot=&aNode;
  }
  else{
    if(aNode<*node2)
      node2->mLeft=&aNode;
    else node2->mRight=&aNode;
  }

  return &aNode;
}


/**
 * Removes given node from tree if present.
 * 
 * @update gess 4/11/98
 * @param  aNode to be found and removed
 * @return ptr to remove node, or NULL
 */
nsNode* nsBTree::Remove(nsNode& aNode){
  nsNode* result=0;  
  nsNode* node3=Find(aNode);

  if(node3) {
    nsNode* node1;
    nsNode* node2;

    if((!node3->mLeft) || (!node3->mRight))
      node2=node3;
    else node2=After(*node3);

    if(node2->mLeft)
      node1=node2->mLeft;
    else node1=node2->mRight;

    if(node1)
      node1->mParent=node2->mParent;

    if(node2->mParent) {
      if(node2==node2->mParent->mLeft)
        node2->mParent->mLeft=node1;
      else node2->mParent->mRight=node1;
    }
    else mRoot=node1;

    if(node2!=node3)
      (*node3)==(*node2);

    if(node2->mColor=nsNode::eBlack)
      ReBalance(*node1);

    delete node2;
    result=&aNode;
  }
  return result;
}

/**
 * Clears the tree of any data. 
 * Be careful here if your objects are heap based!
 * This method doesn't free the objects, so if you
 * don't have your own pointers, they will become
 * orphaned.
 * 
 * @update gess 4/11/98
 * @param  
 * @return this
 */
nsBTree& nsBTree::Empty(nsNode* aNode) {
  mRoot=0;
  return *this;
}

/**
 * This method destroys all the objects in the tree.
 * WARNING: Never call this method on stored objects
 *          that are stack-based!
 * 
 * @update gess 4/11/98
 * @param  
 * @return this
 */
nsBTree& nsBTree::Erase(nsNode* aNode){
  nsNode* node1 =(aNode) ? aNode : mRoot;

  if(aNode) {
    Erase(aNode->mLeft);  //begin by walking left side
    Erase(aNode->mRight); //then search right side
    delete aNode;         //until a leaf, then delete
  }
  return *this;
}

/**
 * Retrieve ptr to first node in tree
 * 
 * @update gess 4/11/98
 * @return 
 */
nsNode* nsBTree::First(void) const{
  if(mRoot)
    return First(*mRoot);
  return 0;
}

/**
 * Retrive ptr to first node rel to given node
 * 
 * @update gess 4/11/98
 * @param  node to begin scan from
 * @return ptr to first node from given node or NULL
 */
nsNode* nsBTree::First(const nsNode& aNode) const{
  nsNode* result=0;

  if(mRoot) {
    result=mRoot;
    while(result->GetLeftNode()) {
      result=result->GetLeftNode();
    }
  }
  return result;
}

/**
 * Retrive ptr to last node 
 * 
 * @update gess 4/11/98
 * @return ptr to last node rel to root or NULL
 */
nsNode* nsBTree::Last(void) const{
  if(mRoot)
    return Last(*mRoot);
  return 0;
}

/**
 * Retrive ptr to last node rel to given node
 * 
 * @update gess 4/11/98
 * @param  node to begin scan from
 * @return ptr to first node from given node or NULL
 */
nsNode* nsBTree::Last(const nsNode& aNode) const{
  nsNode* result=0;

  if(mRoot) {
    result=mRoot;
    while(result->GetRightNode()) {
      result=result->GetRightNode();
    }
  }
  return result;
}


/**
 * Retrive ptr to prior node rel to given node
 * 
 * @update gess 4/11/98
 * @param  node to begin scan from
 * @return ptr to prior node from given node or NULL
 */
nsNode* nsBTree::Before(const nsNode& aNode) const{
  
  if(aNode.GetLeftNode())
    return Last(*aNode.GetLeftNode());

  //otherwise...

  nsNode* node1=(nsNode*)&aNode;
  nsNode* node2=aNode.GetParentNode();
  
  while((node2) && (node1==node2->GetLeftNode())) {
    node1=node2;
    node2=node2->GetParentNode();
  }
  return node2;
}


/**
 * Retrive ptr to next node rel to given node
 * 
 * @update gess 4/11/98
 * @param  node to begin scan from
 * @return ptr to next node from given node or NULL
 */
nsNode* nsBTree::After(const nsNode& aNode) const{
  
  if(aNode.GetRightNode())
    return First(*aNode.GetRightNode());

  //otherwise...

  nsNode* node1=(nsNode*)&aNode;
  nsNode* node2=aNode.GetParentNode();
  
  while((node2) && (node1==node2->GetRightNode())) {
    node1=node2;
    node2=node2->GetParentNode();
  }

  return node2;
}

/**
 * Scan for given node
 * 
 * @update gess 4/11/98
 * @param  node to find
 * @return ptr to given node, or NULL
 */
nsNode* nsBTree::Find(const nsNode& aNode) const{
  nsNode* result=mRoot;

  while((result) && (!(aNode==(*result)))) {
    if(aNode<*result)
      result=result->mLeft;
    else result=result->mRight;
  }
  return (nsNode*)result;
}

/**
 * Rebalances tree around the given node. This only
 * needs to be called after a node is deleted.
 * This method does nothing for btrees, but is
 * needed for RBTrees.
 * 
 * @update gess 4/11/98
 * @param  aNode -- node to balance around
 * @return this
 */
nsBTree& nsBTree::ReBalance(nsNode& aNode){
  return *this;
}


/**
 * 
 * 
 * @update gess 4/11/98
 * @param  
 * @return 
 */
const nsBTree& nsBTree::ForEach(nsNodeFunctor& aFunctor,nsNode* aNode) const{
  nsNode* node1 =(aNode) ? aNode : mRoot;

  if(node1) {
    if(node1->mLeft)
      ForEach(aFunctor,node1->mLeft);  //begin by walking left side
    aFunctor(*node1);
    if(node1->mRight)
      ForEach(aFunctor,node1->mRight); //then search right side
  }
  return *this;
}

