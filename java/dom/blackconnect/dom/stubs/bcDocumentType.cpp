#include "bcDocumentType.h"
#include "bcNamedNodeMap.h"

NS_IMPL_ISUPPORTS1(bcDocumentType, DocumentType)

bcDocumentType::bcDocumentType(nsIDOMDocumentType* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  nodePtr = new bcNode(ptr);
}

bcDocumentType::~bcDocumentType()
{
  /* destructor code */
}

/* readonly attribute DOMString name; */
NS_IMETHODIMP bcDocumentType::GetName(DOMString *aName)
{
  nsString ret;
  nsresult rv = domPtr->GetName(ret);
  *aName = ret.ToNewUnicode();
  return rv;
}


/* readonly attribute NamedNodeMap entities; */
NS_IMETHODIMP bcDocumentType::GetEntities(NamedNodeMap * *aEntities)
{
  nsIDOMNamedNodeMap* ret = nsnull;
  nsresult rv = domPtr->GetEntities(&ret);
  *aEntities = NEW_BCNAMEDNODEMAP(ret);
  return rv;
}


/* readonly attribute NamedNodeMap notations; */
NS_IMETHODIMP bcDocumentType::GetNotations(NamedNodeMap * *aNotations)
{
  nsIDOMNamedNodeMap* ret = nsnull;
  nsresult rv = domPtr->GetNotations(&ret);
  *aNotations = NEW_BCNAMEDNODEMAP(ret);
  return rv;
}


/* readonly attribute DOMString publicId; */
NS_IMETHODIMP bcDocumentType::GetPublicId(DOMString *aPublicId)
{
  nsString ret;
  nsresult rv = domPtr->GetPublicId(ret);
  *aPublicId = ret.ToNewUnicode();
  return rv;
}


/* readonly attribute DOMString systemId; */
NS_IMETHODIMP bcDocumentType::GetSystemId(DOMString *aSystemId)
{
  nsString ret;
  nsresult rv = domPtr->GetSystemId(ret);
  *aSystemId = ret.ToNewUnicode();
  return rv;
}


/* readonly attribute DOMString internalSubset; */
NS_IMETHODIMP bcDocumentType::GetInternalSubset(DOMString *aInternalSubset)
{
  nsString ret;
  nsresult rv = domPtr->GetInternalSubset(ret);
  *aInternalSubset = ret.ToNewUnicode();
  return rv;
}


