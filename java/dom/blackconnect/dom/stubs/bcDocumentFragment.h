#ifndef __bcDocumentFragment_h__
#define __bcDocumentFragment_h__

#include "dom.h"
#include "bcNode.h"
#include "nsIDOMDocumentFragment.h"

#define DOM_DOCUMENTFRAGMENT_PTR(_documentfragment_) (!_documentfragment_ ? nsnull : ((bcDocumentFragment*)_documentfragment_)->domPtr)
#define NEW_BCDOCUMENTFRAGMENT(_documentfragment_) (!_documentfragment_ ? nsnull : new bcDocumentFragment(_documentfragment_))

class bcDocumentFragment : public DocumentFragment
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_DOCUMENTFRAGMENT
  NS_FORWARD_NODE(nodePtr->)

  bcDocumentFragment(nsIDOMDocumentFragment* domPtr);
  virtual ~bcDocumentFragment();
private:
  nsIDOMDocumentFragment* domPtr;
  bcNode* nodePtr;
  /* additional members */
};

#endif //  __bcDocumentFragment_h__
