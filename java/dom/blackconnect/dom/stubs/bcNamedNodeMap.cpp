#include "bcNamedNodeMap.h"
#include "bcNode.h"

NS_IMPL_ISUPPORTS1(bcNamedNodeMap, NamedNodeMap)

bcNamedNodeMap::bcNamedNodeMap(nsIDOMNamedNodeMap* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
}

bcNamedNodeMap::~bcNamedNodeMap()
{
  /* destructor code */
}

/* Node getNamedItem (in DOMString name); */
NS_IMETHODIMP bcNamedNodeMap::GetNamedItem(DOMString name, Node **_retval)
{
  nsString _name(name);
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->GetNamedItem(_name, &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* Node setNamedItem (in Node arg)  raises (DOMException); */
NS_IMETHODIMP bcNamedNodeMap::SetNamedItem(Node *arg, Node **_retval)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->SetNamedItem(DOM_NODE_PTR(arg), &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* Node removeNamedItem (in DOMString name)  raises (DOMException); */
NS_IMETHODIMP bcNamedNodeMap::RemoveNamedItem(DOMString name, Node **_retval)
{
  nsString _name(name);
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->RemoveNamedItem(_name, &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* Node item (in unsigned long index); */
NS_IMETHODIMP bcNamedNodeMap::Item(PRUint32 index, Node **_retval)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->Item(index, &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* readonly attribute unsigned long length; */
NS_IMETHODIMP bcNamedNodeMap::GetLength(PRUint32 *aLength)
{
  return domPtr->GetLength(aLength);
}


/* Node getNamedItemNS (in DOMString namespaceURI, in DOMString localName); */
NS_IMETHODIMP bcNamedNodeMap::GetNamedItemNS(DOMString namespaceURI, DOMString localName, Node **_retval)
{
  nsString _namespaceURI(namespaceURI), _localName(localName);
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->GetNamedItemNS(_namespaceURI, _localName, &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* Node setNamedItemNS (in Node arg)  raises (DOMException); */
NS_IMETHODIMP bcNamedNodeMap::SetNamedItemNS(Node *arg, Node **_retval)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->SetNamedItemNS(DOM_NODE_PTR(arg), &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* Node removeNamedItemNS (in DOMString namespaceURI, in DOMString localName)  raises (DOMException); */
NS_IMETHODIMP bcNamedNodeMap::RemoveNamedItemNS(DOMString namespaceURI, DOMString localName, Node **_retval)
{
  nsString _namespaceURI(namespaceURI), _localName(localName);
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->RemoveNamedItemNS(_namespaceURI, _localName, &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

