#include "bcDOMImplementation.h"
#include "bcDocumentType.h"
#include "bcDocument.h"

NS_IMPL_ISUPPORTS1(bcDOMImplementation, DOMImplementation)

bcDOMImplementation::bcDOMImplementation(nsIDOMDOMImplementation* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
}

bcDOMImplementation::~bcDOMImplementation()
{
  /* destructor code */
}

/* boolean hasFeature (in DOMString feature, in DOMString version); */
NS_IMETHODIMP bcDOMImplementation::HasFeature(DOMString feature, DOMString version, PRBool *_retval)
{
  nsString _feature(feature), _version(version);
  return domPtr->HasFeature(_feature, _version, _retval);
}

/* DocumentType createDocumentType (in DOMString qualifiedName, in DOMString publicId, in DOMString systemId)  raises (DOMException); */
NS_IMETHODIMP bcDOMImplementation::CreateDocumentType(DOMString qualifiedName, DOMString publicId, DOMString systemId, DocumentType **_retval)
{
  nsIDOMDocumentType* ret = nsnull;
  nsString _qualifiedName(qualifiedName), _publicId(publicId), _systemId(systemId);
  nsresult rv = domPtr->CreateDocumentType(_qualifiedName, _publicId, _systemId, &ret);
  *_retval = NEW_BCDOCUMENTTYPE(ret);
  return rv;
}

/* Document createDocument (in DOMString namespaceURI, in DOMString qualifiedName, in DocumentType doctype)  raises (DOMException); */
NS_IMETHODIMP bcDOMImplementation::CreateDocument(DOMString namespaceURI, DOMString qualifiedName, DocumentType *doctype, Document **_retval)
{
  nsIDOMDocument* ret = nsnull;
  nsString _namespaceURI(namespaceURI), _qualifiedName(qualifiedName);
  nsresult rv = domPtr->CreateDocument(_namespaceURI, _qualifiedName, 
				       DOM_DOCUMENTTYPE_PTR(doctype), &ret);
  *_retval = NEW_BCDOCUMENT(ret);
  return rv;
}

