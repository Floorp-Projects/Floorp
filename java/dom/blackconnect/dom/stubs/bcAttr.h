#ifndef __bcAttr_h__
#define __bcAttr_h__

#include "dom.h"
#include "bcNode.h"
#include "nsIDOMAttr.h"

#define DOM_ATTR_PTR(_attr_) (!_attr_ ? nsnull : ((bcAttr*)_attr_)->domPtr)
#define NEW_BCATTR(_attr_) (!_attr_ ? nsnull : new bcAttr(_attr_))

class bcAttr : public Attr
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_ATTR
  NS_FORWARD_NODE(nodePtr->)

  bcAttr(nsIDOMAttr* domPtr);
  virtual ~bcAttr();
  nsIDOMAttr* domPtr;
private:
  bcNode* nodePtr;
  /* additional members */
};

#endif //  __bcAttr_h__
