#include "bcNode.h"
#include "bcNodeList.h"
#include "bcNamedNodeMap.h"
#include "bcDocument.h"
//  #include "nsDOMError.h"
//  #include "nsIDOMEventTarget.h"  

/* Implementation file */
NS_IMPL_ISUPPORTS1(bcNode, Node)

bcNode::bcNode(nsIDOMNode* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
}

bcNode::~bcNode()
{
  /* destructor code */
}

/* readonly attribute DOMString nodeName; */
NS_IMETHODIMP bcNode::GetNodeName(DOMString *aNodeName)
{
  nsString ret;
  nsresult rv = domPtr->GetNodeName(ret);
  *aNodeName = ret.ToNewUnicode();
  return rv;
}

/* attribute DOMString nodeValue; */
NS_IMETHODIMP bcNode::GetNodeValue(DOMString *aNodeValue)
{
  nsString ret;
  nsresult rv = domPtr->GetNodeValue(ret);
  *aNodeValue = ret.ToNewUnicode();
  return rv;
}
NS_IMETHODIMP bcNode::SetNodeValue(DOMString aNodeValue)
{
  nsString val(aNodeValue);
  nsresult rv = domPtr->SetNodeValue(val);
  return rv;
}

/* readonly attribute unsigned short nodeType; */
NS_IMETHODIMP bcNode::GetNodeType(PRUint16 *aNodeType)
{
  PRUint16 type = 0;
  nsresult rv = domPtr->GetNodeType(&type);
  *aNodeType = type;
  return rv;
}

/* readonly attribute Node parentNode; */
NS_IMETHODIMP bcNode::GetParentNode(Node * *aParentNode)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->GetParentNode(&ret);
  *aParentNode = NEW_BCNODE(ret);
  return rv;
}

/* readonly attribute NodeList childNodes; */
NS_IMETHODIMP bcNode::GetChildNodes(NodeList * *aChildNodes)
{
  nsIDOMNodeList* ret = nsnull;
  nsresult rv = domPtr->GetChildNodes(&ret);
  *aChildNodes = NEW_BCNODELIST(ret);
  return rv;
}

/* readonly attribute Node firstChild; */
NS_IMETHODIMP bcNode::GetFirstChild(Node * *aFirstChild)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->GetFirstChild(&ret);
  *aFirstChild = NEW_BCNODE(ret);
  return rv;
}

/* readonly attribute Node lastChild; */
NS_IMETHODIMP bcNode::GetLastChild(Node * *aLastChild)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->GetLastChild(&ret);
  *aLastChild = NEW_BCNODE(ret);
  return rv;
}

/* readonly attribute Node previousSibling; */
NS_IMETHODIMP bcNode::GetPreviousSibling(Node * *aPreviousSibling)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->GetPreviousSibling(&ret);
  *aPreviousSibling = NEW_BCNODE(ret);
  return rv;
}

/* readonly attribute Node nextSibling; */
NS_IMETHODIMP bcNode::GetNextSibling(Node * *aNextSibling)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->GetNextSibling(&ret);
  *aNextSibling = NEW_BCNODE(ret);
  return rv;
}

/* readonly attribute NamedNodeMap attributes; */
NS_IMETHODIMP bcNode::GetAttributes(NamedNodeMap * *aAttributes)
{
  nsIDOMNamedNodeMap* ret = nsnull;
  nsresult rv = domPtr->GetAttributes(&ret);
  *aAttributes = NEW_BCNAMEDNODEMAP(ret);
  return rv;
}

/* readonly attribute Document ownerDocument; */
NS_IMETHODIMP bcNode::GetOwnerDocument(Document * *aOwnerDocument)
{
  nsIDOMDocument* ret;
  nsresult rv = domPtr->GetOwnerDocument(&ret);
  *aOwnerDocument = NEW_BCDOCUMENT(ret);
  return rv;
}

/* Node insertBefore (in Node newChild, in Node refChild)  raises (DOMException); */
NS_IMETHODIMP bcNode::InsertBefore(Node *newChild, Node *refChild, Node **_retval)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->InsertBefore(DOM_NODE_PTR(newChild), DOM_NODE_PTR(refChild), &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* Node replaceChild (in Node newChild, in Node oldChild)  raises (DOMException); */
NS_IMETHODIMP bcNode::ReplaceChild(Node *newChild, Node *oldChild, Node **_retval)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->ReplaceChild(DOM_NODE_PTR(newChild), DOM_NODE_PTR(oldChild), &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* Node removeChild (in Node oldChild)  raises (DOMException); */
NS_IMETHODIMP bcNode::RemoveChild(Node *oldChild, Node **_retval)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->RemoveChild(DOM_NODE_PTR(oldChild), &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* Node appendChild (in Node newChild)  raises (DOMException); */
NS_IMETHODIMP bcNode::AppendChild(Node *newChild, Node **_retval)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->AppendChild(DOM_NODE_PTR(newChild), &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* boolean hasChildNodes (); */
NS_IMETHODIMP bcNode::HasChildNodes(PRBool *_retval)
{
  return domPtr->HasChildNodes(_retval);
}

/* Node cloneNode (in boolean deep); */
NS_IMETHODIMP bcNode::CloneNode(PRBool deep, Node **_retval)
{
  nsIDOMNode* ret = nsnull;
  nsresult rv = domPtr->CloneNode(deep, &ret);
  *_retval = NEW_BCNODE(ret);
  return rv;
}

/* void normalize (); */
NS_IMETHODIMP bcNode::Normalize()
{
    return domPtr->Normalize();
}

/* boolean supports (in DOMString feature, in DOMString version); */
NS_IMETHODIMP bcNode::Supports(DOMString feature, DOMString version, PRBool *_retval)
{
  nsString _feature(feature), _version(version);
  return domPtr->IsSupported(_feature, _version, _retval);
}

/* readonly attribute DOMString namespaceURI; */
NS_IMETHODIMP bcNode::GetNamespaceURI(DOMString *aNamespaceURI)
{
  nsString ret;
  nsresult rv = domPtr->GetNamespaceURI(ret);
  *aNamespaceURI = ret.ToNewUnicode();
  return rv;
}

/* attribute DOMString prefix; */
NS_IMETHODIMP bcNode::GetPrefix(DOMString *aPrefix)
{
  nsString ret;
  nsresult rv = domPtr->GetPrefix(ret);
  *aPrefix = ret.ToNewUnicode();
  return rv;
}
NS_IMETHODIMP bcNode::SetPrefix(DOMString aPrefix)
{
  nsString val(aPrefix);
  nsresult rv = domPtr->SetPrefix(val);
  return rv;
}

/* readonly attribute DOMString localName; */
NS_IMETHODIMP bcNode::GetLocalName(DOMString *aLocalName)
{
  nsString ret;
  nsresult rv = domPtr->GetPrefix(ret);
  *aLocalName = ret.ToNewUnicode();
  return rv;
}

