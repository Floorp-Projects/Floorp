
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

#include "nsRBTree.h"



/**************************************************
  Here comes the nsRBTree class...
 *************************************************/

/**
 * nsRBTree constructor
 * 
 * @update gess 4/11/98
 */
nsRBTree::nsRBTree() : nsBTree() {
  mRoot=0;
}

/**
 * nsRBTree constructor
 * 
 * @update gess 4/11/98
 */
nsRBTree::nsRBTree(const nsRBTree& aCopy) : nsBTree(aCopy) {
  mRoot=aCopy.mRoot;
}

/**
 * nsRBTree destructor
 * 
 * @update gess 4/11/98
 * @param  
 * @return 
 */
nsRBTree::~nsRBTree(){
  if(mRoot){
    //walk the tree and destroy the children.
  }
}


/**
 * Given a node, we're supposed to add it into 
 * our tree.
 * 
 * @update gess 4/11/98
 * @param  
 * @return 
 */
nsNode* nsRBTree::Add(nsNode& aNode){

  nsBTree::Add(aNode);

  nsNode* node1=&aNode;
  nsNode* node2=0;

  node1->mColor=nsNode::eRed;

  while((node1!=mRoot) && (node1->mParent->mColor==nsNode::eRed)) {
    if(node1->mParent==node1->mParent->mParent->mLeft) {
      node2=node1->mParent->mParent->mLeft; 
      if(node2->mColor==nsNode::eRed) {
        node1->mParent->mColor=nsNode::eBlack;
        node2->mColor=nsNode::eBlack;
        node1->mParent->mParent->mColor=nsNode::eRed;
        node1=node1->mParent->mParent;
      }
      else {
        if(node1==node1->mParent->mRight) {
          node1=node1->mParent;
          ShiftLeft(*node1);
        }
        node1->mParent->mColor=nsNode::eBlack;
        node1->mParent->mParent->mColor=nsNode::eRed;
        ShiftRight(*node1->mParent->mParent);
      }
    }
    else {
      node2=node1->mParent->mParent->mRight; 
      if (node2->mColor==nsNode::eRed){
        node1->mParent->mColor=nsNode::eBlack;
        node2->mColor=nsNode::eBlack;
        node1->mParent->mParent->mColor=nsNode::eRed;
        node1=node1->mParent->mParent;
      }
      else {
        if (node1==node1->mParent->mLeft) {
          node1=node1->mParent;
          ShiftRight(*node1);
        }
        node1->mParent->mColor=nsNode::eBlack;
        node1->mParent->mParent->mColor=nsNode::eRed;
        ShiftLeft(*node1->mParent->mParent);
      }
    }
  }

  mRoot->mColor=nsNode::eBlack;
  return &aNode;
}


/**
 * Retrive the first node in the tree
 * 
 * @update gess 4/11/98
 * @param  
 * @return 
 */
nsNode* nsRBTree::First(){
  nsNode* result=First(*mRoot);
  return result;
}

/**
 * Retrieve the first node given a starting node
 * 
 * @update gess 4/11/98
 * @param  aNode --
 * @return node ptr or null
 */
nsNode* nsRBTree::First(nsNode& aNode){
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
 * Find the last node in the tree
 * 
 * @update gess 4/11/98
 * @param  
 * @return node ptr or null
 */
nsNode* nsRBTree::Last(){
  nsNode* result=Last(*mRoot);
  return result;
}

/**
 * Find the last node from a given node
 * 
 * @update gess 4/11/98
 * @param  aNode -- node ptr to start from
 * @return node ptr or null
 */
nsNode* nsRBTree::Last(nsNode& aNode){
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
 * Retrieve the node that preceeds the given node
 * 
 * @update gess 4/11/98
 * @param  aNode -- node to find precedent of
 * @return preceeding node ptr, or null
 */
nsNode* nsRBTree::Before(nsNode& aNode){
  
  if(aNode.GetLeftNode())
    return Last(*aNode.GetLeftNode());

  //otherwise...

  nsNode* node1=&aNode;
  nsNode* node2=aNode.GetParentNode();
  
  while((node2) && (node1==node2->GetLeftNode())) {
    node1=node2;
    node2=node2->GetParentNode();
  }
  return node2;
}


/**
 * Retrieve a ptr to the node following the given node
 * 
 * @update gess 4/11/98
 * @param  aNode -- node to find successor node from
 * @return node ptr or null
 */
nsNode* nsRBTree::After(nsNode& aNode){
  
  if(aNode.GetRightNode())
    return First(*aNode.GetRightNode());

  //otherwise...

  nsNode* node1=&aNode;
  nsNode* node2=aNode.GetParentNode();
  
  while((node2) && (node1==node2->GetRightNode())) {
    node1=node2;
    node2=node2->GetParentNode();
  }

  return node2;
}

/**
 * Find a (given) node in the tree
 * 
 * @update gess 4/11/98
 * @param  node to find in the tree
 * @return node ptr (if found) or null
 */
nsNode* nsRBTree::Find(nsNode& aNode){
  nsNode* result=mRoot;

  while((result) && (!((*result)==aNode))) {
    if(aNode<*result)
      result=result->mLeft;
    else result=result->mRight;
  }
  return result;
}


/**
 * Causes a shift to the left, to keep the
 * underlying RB data in balance
 * 
 * @update gess 4/11/98
 * @param  
 * @return this
 */
nsRBTree& nsRBTree::ShiftLeft(nsNode& aNode){

  nsNode* temp= aNode.mRight;

  aNode.mRight=temp->mLeft;
  if(temp->mLeft)
    temp->mRight->mParent=&aNode;
  temp->mParent= aNode.mParent;
  if (aNode.mParent) {
    if (&aNode==aNode.mParent->mLeft)
      aNode.mParent->mLeft=temp;
    else aNode.mParent->mRight=temp;
  }
  else mRoot=temp;
  temp->mLeft=&aNode;;
  aNode.mParent=temp;
  return *this;
}

/**
 * Causes a shift right to occur, to keep the
 * underlying RB data in balance
 * 
 * @update gess 4/11/98
 * @param  aNode -- node at which to perform shift
 * @return this
 */
nsRBTree& nsRBTree::ShiftRight(nsNode& aNode){
  
  nsNode* temp=aNode.mLeft;

  aNode.mLeft=temp->mRight;
  if(temp->mRight)
    temp->mRight->mParent=&aNode;
  temp->mParent=aNode.mParent;
  if(aNode.mParent){
    if(&aNode==aNode.mParent->mRight)
      aNode.mParent->mRight=temp;
    else aNode.mParent->mLeft=temp;
    }
  else mRoot=temp;
  temp->mRight=&aNode;
  aNode.mParent=temp;
  return *this;
}

/**
 * Rebalances tree around the given node. This only
 * needs to be called after a node is deleted.
 * 
 * @update gess 4/11/98
 * @param  aNode -- node to balance around
 * @return this
 */
nsBTree& nsRBTree::ReBalance(nsNode& aNode){

  nsNode* node1=&aNode;
  nsNode* node2=0;

  while ((node1!=mRoot) && (node1->mColor==nsNode::eBlack)) {
    if(node1==node1->mParent->mLeft) {
      node2=node1->mParent->mRight;  
      if(node2->mColor==nsNode::eRed) {
        node2->mColor=nsNode::eBlack;
        node1->mParent->mColor=nsNode::eRed;
        ShiftLeft(*node1->mParent);
        node2=node1->mParent->mRight;
      }
    
      if((node2->mLeft->mColor == nsNode::eBlack) &&
	 (node2->mRight->mColor == nsNode::eBlack)) {
        node2->mColor=nsNode::eRed;
        node1=node1->mParent;
      } 
      else {
        if(node2->mRight->mColor == nsNode::eBlack) {
          node2->mLeft->mColor=nsNode::eBlack;
          node2->mColor=nsNode::eRed;
          ShiftRight(*node2);
          node2=node1->mParent->mRight;
        }

        node2->mColor=node1->mParent->mColor;
        node1->mParent->mColor=nsNode::eBlack;
        node2->mRight->mColor=nsNode::eBlack;
        ShiftLeft(*node1->mParent);
        node1=mRoot;
      } //else
    }
    else {
      node2=node1->mParent->mLeft;
      if(node2->mColor==nsNode::eRed) {
        node2->mColor=nsNode::eBlack;
        node1->mParent->mColor=nsNode::eRed;
        ShiftRight(*node1->mParent);
        node2=node1->mParent->mLeft;
      }

      if((node2->mRight->mColor == nsNode::eBlack) &&
	 (node2->mLeft->mColor == nsNode::eBlack)) {
        node2->mColor=nsNode::eRed;
        node1=node1->mParent;
      }
      else {
        if(node2->mLeft->mColor == nsNode::eBlack){
          node2->mRight->mColor=nsNode::eBlack;
          node2->mColor=nsNode::eRed;
          ShiftLeft(*node2);
          node2=node1->mParent->mLeft;
        }
      
        node2->mColor=node1->mParent->mColor;
        node1->mParent->mColor=nsNode::eBlack;
        node2->mLeft->mColor=nsNode::eBlack;
        ShiftRight(*node1->mParent);
        node1=mRoot;
      } //else
    } //if
  } //while

  node1->mColor=nsNode::eBlack;
  return *this;
}

/**************************************************
  Here comes the nsRBTreeIterator class...
 *************************************************/

/**
 * 
 * 
 * @update gess 4/11/98
 * @param  
 * @return 
 */
nsRBTreeIterator::nsRBTreeIterator(const nsRBTree& aTree) : mTree(aTree) {
}

/**
 * copy constructor
 * 
 * @update gess 4/11/98
 * @param  aCopy is the object you want to copy from
 * @return newly constructed object
 */
nsRBTreeIterator::nsRBTreeIterator(const nsRBTreeIterator& aCopy) : mTree(aCopy.mTree) {
}

/**
 * Destructor method
 * 
 * @update gess 4/11/98
 */
nsRBTreeIterator::~nsRBTreeIterator(){
}

/**
 * This method iterates over the tree, calling
 * aFunctor for each node.
 * 
 * @update gess 4/11/98
 * @param  aFunctor -- object to call for each node
 * @param  aNode -- node at which to start iteration
 * @return this
 */
const nsRBTreeIterator& nsRBTreeIterator::ForEach(nsNodeFunctor& aFunctor) const{
  mTree.ForEach(aFunctor);
  return *this;
}
