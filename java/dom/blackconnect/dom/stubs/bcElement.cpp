#include "bcElement.h"
#include "bcAttr.h"
#include "bcNodeList.h"

NS_IMPL_ISUPPORTS1(bcElement, Element)

bcElement::bcElement(nsIDOMElement* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  nodePtr = new bcNode(ptr);
}

bcElement::~bcElement()
{
  /* destructor code */
}

/* readonly attribute DOMString tagName; */
NS_IMETHODIMP bcElement::GetTagName(DOMString *aTagName)
{
  nsString ret;
  nsresult rv = domPtr->GetTagName(ret);
  *aTagName = ret.ToNewUnicode();
  return rv;
}

/* DOMString getAttribute (in DOMString name); */
NS_IMETHODIMP bcElement::GetAttribute(DOMString name, DOMString *_retval)
{
  nsString ret, _name(name);
  nsresult rv = domPtr->GetAttribute(_name, ret);
  *_retval = ret.ToNewUnicode();
  return rv;
}

/* void setAttribute (in DOMString name, in DOMString value)  raises (DOMException); */
NS_IMETHODIMP bcElement::SetAttribute(DOMString name, DOMString value)
{
  nsString _name(name), _value(value);
  nsresult rv = domPtr->SetAttribute(_name, _value);
  printf("\n===== setting value (%s) to attr (%s)\n\n", 
	 _value.ToNewCString(),
	 _name.ToNewCString());
  return rv;
}

/* void removeAttribute (in DOMString name)  raises (DOMException); */
NS_IMETHODIMP bcElement::RemoveAttribute(DOMString name)
{
  nsString _name(name);
  return domPtr->RemoveAttribute(_name);
}

/* Attr getAttributeNode (in DOMString name); */
NS_IMETHODIMP bcElement::GetAttributeNode(DOMString name, Attr **_retval)
{
  nsString _name(name);
  nsIDOMAttr* ret = nsnull;
  nsresult rv = domPtr->GetAttributeNode(_name, &ret);
  *_retval = NEW_BCATTR(ret);
  return rv;
}

/* Attr setAttributeNode (in Attr newAttr)  raises (DOMException); */
NS_IMETHODIMP bcElement::SetAttributeNode(Attr *newAttr, Attr **_retval)
{
  nsIDOMAttr* ret = nsnull;
  nsresult rv = domPtr->SetAttributeNode(DOM_ATTR_PTR(newAttr), &ret);
  *_retval = NEW_BCATTR(ret);
  return rv;
}

/* Attr removeAttributeNode (in Attr oldAttr)  raises (DOMException); */
NS_IMETHODIMP bcElement::RemoveAttributeNode(Attr *oldAttr, Attr **_retval)
{
  nsIDOMAttr* ret = nsnull;
  nsresult rv = domPtr->RemoveAttributeNode(DOM_ATTR_PTR(oldAttr), &ret);
  *_retval = NEW_BCATTR(ret);
  return rv;
}

/* NodeList getElementsByTagName (in DOMString name); */
NS_IMETHODIMP bcElement::GetElementsByTagName(DOMString name, NodeList **_retval)
{
  nsIDOMNodeList* ret = nsnull;
  nsString _name(name);
  nsresult rv = domPtr->GetElementsByTagName(_name, &ret);
  *_retval = NEW_BCNODELIST(ret);
  return rv;
}

/* DOMString getAttributeNS (in DOMString namespaceURI, in DOMString localName); */
NS_IMETHODIMP bcElement::GetAttributeNS(DOMString namespaceURI, DOMString localName, DOMString *_retval)
{
  nsString _namespaceURI(namespaceURI), _localName(localName), ret;
  nsresult rv = domPtr->GetAttributeNS(_namespaceURI, _localName, ret);
  *_retval = ret.ToNewUnicode();
  return rv;
}

/* void setAttributeNS (in DOMString namespaceURI, in DOMString qualifiedName, in DOMString value)  raises (DOMException); */
NS_IMETHODIMP bcElement::SetAttributeNS(DOMString namespaceURI, DOMString qualifiedName, DOMString value)
{
  nsString _namespaceURI(namespaceURI), _qualifiedName(qualifiedName), _value(value);
  return domPtr->SetAttributeNS(_namespaceURI, _qualifiedName, _value);
}

/* void removeAttributeNS (in DOMString namespaceURI, in DOMString localName)  raises (DOMException); */
NS_IMETHODIMP bcElement::RemoveAttributeNS(DOMString namespaceURI, DOMString localName)
{
  nsString _namespaceURI(namespaceURI), _localName(localName);
  return domPtr->RemoveAttributeNS(_namespaceURI, _localName);
}

/* Attr getAttributeNodeNS (in DOMString namespaceURI, in DOMString localName); */
NS_IMETHODIMP bcElement::GetAttributeNodeNS(DOMString namespaceURI, DOMString localName, Attr **_retval)
{
  nsString _namespaceURI(namespaceURI), _localName(localName);
  nsIDOMAttr* ret = nsnull;
  nsresult rv = domPtr->GetAttributeNodeNS(_namespaceURI, _localName, &ret);
  *_retval = NEW_BCATTR(ret);
  return rv;
}

/* Attr setAttributeNodeNS (in Attr newAttr)  raises (DOMException); */
NS_IMETHODIMP bcElement::SetAttributeNodeNS(Attr *newAttr, Attr **_retval)
{
  nsIDOMAttr* ret = nsnull;
  nsresult rv = domPtr->SetAttributeNodeNS(DOM_ATTR_PTR(newAttr), &ret);
  *_retval = NEW_BCATTR(ret);
  return rv;
}

/* NodeList getElementsByTagNameNS (in DOMString namespaceURI, in DOMString localName); */
NS_IMETHODIMP bcElement::GetElementsByTagNameNS(DOMString namespaceURI, DOMString localName, NodeList **_retval)
{
  nsIDOMNodeList* ret = nsnull;
  nsString _namespaceURI(namespaceURI), _localName(localName);
  nsresult rv = domPtr->GetElementsByTagNameNS(_namespaceURI, _localName, &ret);
  *_retval = NEW_BCNODELIST(ret);
  return rv;
}

/* boolean hasAttribute (in DOMString name); */
NS_IMETHODIMP bcElement::HasAttribute(DOMString name, PRBool *_retval)
{
  nsString _name(name);
  return domPtr->HasAttribute(_name, _retval);
}

/* boolean hasAttributeNS (in DOMString namespaceURI, in DOMString localName); */
NS_IMETHODIMP bcElement::HasAttributeNS(DOMString namespaceURI, DOMString localName, PRBool *_retval)
{
  nsString _namespaceURI(namespaceURI), _localName(localName);
  return domPtr->HasAttributeNS(_namespaceURI, _localName, _retval);
}

