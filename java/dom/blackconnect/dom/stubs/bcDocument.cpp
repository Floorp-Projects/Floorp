#include "bcDocument.h"
#include "bcElement.h"
#include "bcNodeList.h"
#include "bcAttr.h"
#include "bcText.h"
#include "bcComment.h"
#include "bcCDATASection.h"
#include "bcEntityReference.h"
#include "bcDocumentType.h"
#include "bcDocumentFragment.h"
#include "bcProcessingInstruction.h"
#include "bcDOMImplementation.h"

NS_IMPL_ISUPPORTS1(bcDocument, Document)

bcDocument::bcDocument(nsIDOMDocument* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  nodePtr = new bcNode(ptr);
}

bcDocument::~bcDocument()
{
  /* destructor code */
}

/* readonly attribute DocumentType doctype; */
NS_IMETHODIMP bcDocument::GetDoctype(DocumentType * *aDoctype)
{
  nsIDOMDocumentType* ret;
  nsresult rv = domPtr->GetDoctype(&ret);
  *aDoctype = NEW_BCDOCUMENTTYPE(ret);
  return rv;
}


/* readonly attribute DOMImplementation implementation; */
NS_IMETHODIMP bcDocument::GetImplementation(DOMImplementation * *aImplementation)
{
  nsIDOMDOMImplementation* ret;
  nsresult rv = domPtr->GetImplementation(&ret);
  *aImplementation = NEW_BCDOMIMPLEMENTATION(ret);
  return rv;
}


/* readonly attribute Element documentElement; */
NS_IMETHODIMP bcDocument::GetDocumentElement(Element * *aDocumentElement)
{
  nsIDOMElement* ret = nsnull;
  nsresult rv = domPtr->GetDocumentElement(&ret);
  *aDocumentElement = NEW_BCELEMENT(ret);
  return rv;
}


/* Element createElement (in DOMString tagName)  raises (DOMException); */
NS_IMETHODIMP bcDocument::CreateElement(DOMString tagName, Element **_retval)
{
  nsIDOMElement* ret = nsnull;
  nsString _tagName(tagName);
  nsresult rv = domPtr->CreateElement(_tagName, &ret);
  *_retval = NEW_BCELEMENT(ret);
  return rv;
}

/* DocumentFragment createDocumentFragment (); */
NS_IMETHODIMP bcDocument::CreateDocumentFragment(DocumentFragment **_retval)
{
  nsIDOMDocumentFragment* ret;
  nsresult rv = domPtr->CreateDocumentFragment(&ret);
  *_retval = NEW_BCDOCUMENTFRAGMENT(ret);
  return rv;
}

/* Text createTextNode (in DOMString data); */
NS_IMETHODIMP bcDocument::CreateTextNode(DOMString data, Text **_retval)
{
  nsString _data(data);
  nsIDOMText* ret = nsnull;
  nsresult rv = domPtr->CreateTextNode(_data, &ret);
  *_retval = NEW_BCTEXT(ret);
  return rv;
}

/* Comment createComment (in DOMString data); */
NS_IMETHODIMP bcDocument::CreateComment(DOMString data, Comment **_retval)
{
  nsString _data(data);
  nsIDOMComment* ret = nsnull;
  nsresult rv = domPtr->CreateComment(_data, &ret);
  *_retval = NEW_BCCOMMENT(ret);
  return rv;
}

/* CDATASection createCDATASection (in DOMString data)  raises (DOMException); */
NS_IMETHODIMP bcDocument::CreateCDATASection(DOMString data, CDATASection **_retval)
{
  nsString _data(data);
  nsIDOMCDATASection* ret = nsnull;
  nsresult rv = domPtr->CreateCDATASection(_data, &ret);
  *_retval = NEW_BCCDATASECTION(ret);
  return rv;
}

/* ProcessingInstruction createProcessingInstruction (in DOMString target, in DOMString data)  raises (DOMException); */
NS_IMETHODIMP bcDocument::CreateProcessingInstruction(DOMString target, DOMString data, ProcessingInstruction **_retval)
{
  nsIDOMProcessingInstruction* ret;
  nsString _target(target), _data(data);
  nsresult rv = domPtr->CreateProcessingInstruction(_target, _data, &ret);
  *_retval = NEW_BCPROCESSINGINSTRUCTION(ret);
  return rv;
}

/* Attr createAttribute (in DOMString name)  raises (DOMException); */
NS_IMETHODIMP bcDocument::CreateAttribute(DOMString name, Attr **_retval)
{
  nsString _name(name);
  nsIDOMAttr* ret = nsnull;
  nsresult rv = domPtr->CreateAttribute(_name, &ret);
  *_retval = NEW_BCATTR(ret);
  return rv;
}

/* EntityReference createEntityReference (in DOMString name)  raises (DOMException); */
NS_IMETHODIMP bcDocument::CreateEntityReference(DOMString name, EntityReference **_retval)
{
  nsString _name(name);
  nsIDOMEntityReference* ret = nsnull;
  nsresult rv = domPtr->CreateEntityReference(_name, &ret);
  *_retval = NEW_BCENTITYREFERENCE(ret);
  return rv;
}

/* NodeList getElementsByTagName (in DOMString tagname); */
NS_IMETHODIMP bcDocument::GetElementsByTagName(DOMString tagname, NodeList **_retval)
{
  nsString _tagname(tagname);
  nsIDOMNodeList* ret = nsnull;
  nsresult rv = domPtr->GetElementsByTagName(_tagname, &ret);
  *_retval = NEW_BCNODELIST(ret);
  return rv;
}

/* Node importNode (in Node importedNode, in boolean deep)  raises (DOMException); */
NS_IMETHODIMP bcDocument::ImportNode(Node *importedNode, PRBool deep, Node **_retval)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->ImportNode(DOM_NODE_PTR(importedNode), deep, &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* Element createElementNS (in DOMString namespaceURI, in DOMString qualifiedName)  raises (DOMException); */
NS_IMETHODIMP bcDocument::CreateElementNS(DOMString namespaceURI, DOMString qualifiedName, Element **_retval)
{
  nsString _namespaceURI(namespaceURI), _qualifiedName(qualifiedName);
  nsIDOMElement* ret = nsnull;
  nsresult rv = domPtr->CreateElementNS(_namespaceURI, _qualifiedName, &ret);
  *_retval = NEW_BCELEMENT(ret);
  return rv;
}

/* Attr createAttributeNS (in DOMString namespaceURI, in DOMString qualifiedName)  raises (DOMException); */
NS_IMETHODIMP bcDocument::CreateAttributeNS(DOMString namespaceURI, DOMString qualifiedName, Attr **_retval)
{
  nsString _namespaceURI(namespaceURI), _qualifiedName(qualifiedName);
  nsIDOMAttr* ret = nsnull;
  nsresult rv = domPtr->CreateAttributeNS(_namespaceURI, _qualifiedName, &ret);
  *_retval = NEW_BCATTR(ret);
  return rv;
}

/* NodeList getElementsByTagNameNS (in DOMString namespaceURI, in DOMString localName); */
NS_IMETHODIMP bcDocument::GetElementsByTagNameNS(DOMString namespaceURI, DOMString localName, NodeList **_retval)
{
  nsString _namespaceURI(namespaceURI), _localName(localName);
  nsIDOMNodeList* ret;
  nsresult rv = domPtr->GetElementsByTagNameNS(_namespaceURI, _localName, &ret);
  *_retval = NEW_BCNODELIST(ret);
  return rv;
}

/* Element getElementById (in DOMString elementId); */
NS_IMETHODIMP bcDocument::GetElementById(DOMString elementId, Element **_retval)
{
  nsString _elementId(elementId);
  nsIDOMElement* ret = nsnull;
  nsresult rv = domPtr->GetElementById(_elementId, &ret);
  *_retval = NEW_BCELEMENT(ret);
  return rv;
}

