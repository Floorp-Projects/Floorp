#ifndef __bcNode_h__
#define __bcNode_h__

#include "dom.h"
#include "nsIDOMNode.h"

#define DOM_NODE_PTR(_node_) (!_node_ ? nsnull : ((bcNode*)_node_)->domPtr)
#define NEW_BCNODE(_node_) (!_node_ ? nsnull : new bcNode(_node_))

class bcNode : public Node
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NODE

  bcNode(nsIDOMNode* domPtr);
  virtual ~bcNode();
/*  private: */
  nsIDOMNode* domPtr;
  /* additional members */
};

#endif //  __bcNode_h__
