#ifndef __bcEntityReference_h__
#define __bcEntityReference_h__

#include "dom.h"
#include "bcNode.h"
#include "nsIDOMEntityReference.h"

#define DOM_ENTITYREFERENCE_PTR(_entityreference_) (!_entityreference_ ? nsnull : ((bcEntityReference*)_entityreference_)->domPtr)
#define NEW_BCENTITYREFERENCE(_entityreference_) (!_entityreference_ ? nsnull : new bcEntityReference(_entityreference_))

class bcEntityReference : public EntityReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_ENTITYREFERENCE
  NS_FORWARD_NODE(nodePtr->)

  bcEntityReference(nsIDOMEntityReference* domPtr);
  virtual ~bcEntityReference();
private:
  nsIDOMEntityReference* domPtr;
  bcNode* nodePtr;
  /* additional members */
};

#endif //  __bcEntityReference_h__
