#ifndef __bcElement_h__
#define __bcElement_h__

#include "dom.h"
#include "bcNode.h"
#include "nsIDOMElement.h"

#define DOM_ELEMENT_PTR(_element_) (!_element_ ? nsnull : ((bcElement*)_element_)->domPtr)
#define NEW_BCELEMENT(_element_) (!_element_ ? nsnull : new bcElement(_element_))

class bcElement : public Element
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_ELEMENT
  NS_FORWARD_NODE(nodePtr->)

  bcElement(nsIDOMElement* domPtr);
  virtual ~bcElement();
private:
  nsIDOMElement* domPtr;
  bcNode* nodePtr;
  /* additional members */
};

#endif //  __bcElement_h__
