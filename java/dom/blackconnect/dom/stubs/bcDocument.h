#ifndef __bcDocument_h__
#define __bcDocument_h__

#include "dom.h"
#include "bcNode.h"
#include "nsIDOMDocument.h"

#define DOM_DOCUMENT_PTR(_document_) (!_document_ ? nsnull : ((bcDocument*)_document_)->domPtr)
#define NEW_BCDOCUMENT(_document_) (!_document_ ? nsnull : new bcDocument(_document_))

class bcDocument : public Document
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_DOCUMENT
  NS_FORWARD_NODE(nodePtr->)

  bcDocument(nsIDOMDocument* domPtr);
  virtual ~bcDocument();
private:
  nsIDOMDocument* domPtr;
  bcNode* nodePtr;
  /* additional members */
};

#endif //  __bcDocument_h__
