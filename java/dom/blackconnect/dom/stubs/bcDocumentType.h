#ifndef __bcDocumentType_h__
#define __bcDocumentType_h__

#include "dom.h"
#include "bcNode.h"
#include "nsIDOMDocumentType.h"

#define DOM_DOCUMENTTYPE_PTR(_documenttype_) (!_documenttype_ ? nsnull : ((bcDocumentType*)_documenttype_)->domPtr)
#define NEW_BCDOCUMENTTYPE(_documenttype_) (!_documenttype_ ? nsnull : new bcDocumentType(_documenttype_))

class bcDocumentType : public DocumentType
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_DOCUMENTTYPE
  NS_FORWARD_NODE(nodePtr->)

  bcDocumentType(nsIDOMDocumentType* domPtr);
  virtual ~bcDocumentType();
  nsIDOMDocumentType* domPtr;
private:
  bcNode* nodePtr;
  /* additional members */
};

#endif //  __bcDocumentType_h__
