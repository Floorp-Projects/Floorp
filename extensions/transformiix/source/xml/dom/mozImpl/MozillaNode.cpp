/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */
// Tom Kneeland (3/29/99)
//
//  Wrapper class to convert Mozilla's Node Interface to TransforMIIX's Node
//  Interface.
//
//  NOTE:  The objects being wrapped are not deleted.  It is assumed that they
//         will be deleted when the actual ("real") Mozilla Document is
//         destroyed
//
//         Also note that this object's parent Document provides the necessary
//         factory functions for creating wrapper classes; such as:
//            
//

// Modification History:
// Who  When        What
//

#include "mozilladom.h"
#include "nsCOMPtr.h"
#include "nsIXMLContent.h"

Node::Node()
{
  ownerDocument = NULL;
}

Node::Node(nsIDOMNode* node, Document* owner)
{
  ownerDocument = owner;
  nsNode = node;
}

Node::~Node()
{
}

void Node::setNSObj(nsIDOMNode* node)
{
  //First we must remove this wrapper from the document hash table since we 
  //don't want to be associated with the existing nsIDOM* object anymore
  ownerDocument->removeWrapper((Int32)nsNode);

  //Now assume control of the new node
  nsNode = node;

  //Finally, place our selves back in the hash table, using the new object
  //as the hash value
  ownerDocument->addWrapper(this, (Int32)node);
}

void Node::setNSObj(nsIDOMNode* node, Document* owner)
{
  ownerDocument = owner;
  nsNode = node;
}

nsIDOMNode* Node::getNSObj()
{
  return nsNode;
}

//
//Call nsIDOMNode::GetNodeName, store the results in the nodeName DOMString,
//and return it to the caller.
//
const DOMString& Node::getNodeName()
{
  nsresult result;
  nsCOMPtr<nsIXMLContent> nsXMLContent = do_QueryInterface(nsNode);
  nsCOMPtr<nsIAtom> theNamespaceAtom;

  if (nsNode == NULL)
    return NULL_STRING;

  nsNode->GetNodeName(nodeName.getNSString());
  if (nsXMLContent) {
    result = nsXMLContent->GetNameSpacePrefix(*getter_AddRefs(theNamespaceAtom));
    if (theNamespaceAtom && NS_SUCCEEDED(result)) {
      DOMString theNamespacePrefix;

      theNamespaceAtom->ToString(theNamespacePrefix.getNSString());
      nodeName.insert(0, ":");
      nodeName.insert(0, theNamespacePrefix);
    }
  }

  return nodeName;
}

//
//Call nsIDOMNode::GetNodeValue, store the results in nodeValue, and
//return it to the caller.
//
const DOMString& Node::getNodeValue()
{
  if (nsNode == NULL)
    return NULL_STRING;

  nsNode->GetNodeValue(nodeValue.getNSString());

  return nodeValue;
}

//
//Call nsIDOMNode::GetNodeType passing it a temporary unsigned short.  Then
//pass the value stored in that variable to the caller.
//
unsigned short Node::getNodeType() const
{
  unsigned short nodeType;

  if (nsNode == NULL)
    return 0;

  nsNode->GetNodeType(&nodeType);

  return nodeType;
}

//
//Call nsIDOMNode::GetParentNode(nsIDOMNode**) passing it a handle to a
//nsIDOMNode.  Store the returned nsIDOMNode in the parentNode wrapper object
//and return its address to the caller.
//
Node* Node::getParentNode()
{
  nsIDOMNode* tmpParent = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->GetParentNode(&tmpParent) == NS_OK)
    return ownerDocument->createWrapper(tmpParent);
  else
    return NULL;
}

//
//Call nsIDOMNode::GetChildNodes(nsIDOMNodeList**) passing it a handle to a
//nsIDOMNodeList.  Defer to the owner document to produce a wrapper for this
//object.
//
NodeList* Node::getChildNodes()
{
  nsIDOMNodeList* tmpNodeList = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->GetChildNodes(&tmpNodeList) == NS_OK)
    return ownerDocument->createNodeList(tmpNodeList);
  else
    return NULL;
}

//
//Call nsIDOMNode::GetFirstChild(nsIDOMNode**) passing it a handle to a
//nsIDOMNode.  Defer to the owner document to produce a wrapper for this object.
//
Node* Node::getFirstChild()
{
  nsIDOMNode* tmpFirstChild = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->GetFirstChild(&tmpFirstChild) == NS_OK)
    return ownerDocument->createWrapper(tmpFirstChild);
  else
    return NULL;
}

//
//Call nsIDOMNode::GetLastChild(nsIDOMNode**) passing it a handle to a
//nsIDOMNode.  Defer to the owner document to produce a wrapper for this object.
//
Node* Node::getLastChild() 
{
  nsIDOMNode* tmpLastChild = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->GetLastChild(&tmpLastChild) == NS_OK)
    return ownerDocument->createWrapper(tmpLastChild);
  else
    return NULL;
}

//
//Call nsIDOMNode::GetPreviousSibling(nsIDOMNode**) passing it a handle to a
//nsIDOMNode.  Defer to the owner document to produce a wrapper for this object.
//
Node* Node::getPreviousSibling()
{
  nsIDOMNode* tmpPrevSib = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->GetPreviousSibling(&tmpPrevSib) == NS_OK)
    return ownerDocument->createWrapper(tmpPrevSib);
  else
    return NULL;
}

//
//Call nsIDOMNode::GetNextSibling(nsIDOMNode**) passing it a handle to a
//nsIDOMNode.  Defer to the owner document to produce a wrapper for this object.
//
Node* Node::getNextSibling()
{
  nsIDOMNode* tmpNextSib = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->GetNextSibling(&tmpNextSib) == NS_OK)
      return ownerDocument->createWrapper(tmpNextSib);
  else
    return NULL;
}

//
//Call nsIDOMNode::GetAttributes(nsIDOMNamedNodeMap**) passing it a handle to a
//nsIDOMNamedNodeMap.  Defer to the owner document to produce a wrapper for this
//object.
//
NamedNodeMap* Node::getAttributes()
{
  nsIDOMNamedNodeMap* tmpAttributes = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->GetAttributes(&tmpAttributes) == NS_OK)
    return ownerDocument->createNamedNodeMap(tmpAttributes);
  else
    return NULL;
}

//
//Call nsIDOMNode::GetOwnerDocument(nsIDOMDocument**) passing it a handle to a
//nsIDOMDocument.  ????
//
Document* Node::getOwnerDocument()
{
  /*nsIDOMDocument* tmpOwnerDoc = NULL;

  if (nsNode == NULL)
    return NULL;

  nsNode->GetOwnerDocument(&tmpOwnerDoc);
  ownerDocument->setNSObj(tmpOwnerDoc); */

  return ownerDocument;
}

//
//Call nsIDOMNode::SetNodeValue(nsString*) passing it the nsString wrapped by
//the provided DOMString.
//
void Node::setNodeValue(const DOMString& newNodeValue)
{
  if (nsNode != NULL)
    nsNode->SetNodeValue(newNodeValue.getConstNSString());
}

//
//Retreive the nsIDOMNode objects wrapped by newChild and refChild and pass
//them to nsIDOMNode::InsertBefore(...).  If the return value from InsertBefore
//is valid, retrieve or create a wrapper class for it from the owner document.
//This ensures there newChild is properly hashed (it should have been when it
//was created.)
//
Node* Node::insertBefore(Node* newChild,
                         Node* refChild)
{
  nsIDOMNode* returnValue = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->InsertBefore(newChild->getNSObj(), refChild->getNSObj(),
			   &returnValue) == NS_OK)
    return ownerDocument->createWrapper(returnValue);
  else
    return NULL;
}

//
//Retreive the nsIDOMNode objects wrapped by newChild and oldChild and pass
//them to nsIDOMNode::ReplaceChild(...).  If the replace call success, then
//we want to remove the old child's wrapper object from the hash and return
//it to the caller.  This ensures that when the caller deletes the memory,
//no hash conflicts occure when the address is reused.
//
Node* Node::replaceChild(Node* newChild,
                         Node* oldChild)
{
  nsIDOMNode* returnValue = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->ReplaceChild(newChild->getNSObj(), oldChild->getNSObj(),
			   &returnValue) == NS_OK)
    {
      //We want to remove the wrapper class from the hash table, and return
      //it to the caller.
      return (Node*)ownerDocument->removeWrapper((Int32)returnValue);
    }
  else
    return NULL;
}

//
//Retreive the nsIDOMNode object wrapped by oldChild and pass it to
//nsIDOMNode::RemoveChild(...).  If the return value from RemoveChild
//is valid, then we want to remove the wrapper class from from the hash.
//
Node* Node::removeChild(Node* oldChild)
{
  nsIDOMNode* returnValue = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->RemoveChild(oldChild->getNSObj(), &returnValue) == NS_OK)
    return (Node*)ownerDocument->removeWrapper((Int32)returnValue);
  else
    return NULL;
}

//
//Retreive the nsIDOMNode object wrapped by newChild and pass it to
//nsIDOMNode::AppendChild(...).  If the return value from AppendChild
//is valid, then goto the owner document for a wrapper object for the return
//value.
//
Node* Node::appendChild(Node* newChild)
{
  nsIDOMNode* returnValue = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->AppendChild(newChild->getNSObj(), &returnValue) == NS_OK)
    return ownerDocument->createWrapper(returnValue);
  else
    return NULL;
}

//
//Call nsIDOMNode::Clone to clone the node wrapped by this object.  Simply
//take the returned node, and wrap it in dest.
//
Node* Node::cloneNode(MBool deep, Node* dest)
{
  nsIDOMNode* returnValue = NULL;

  if (nsNode == NULL)
    return NULL;

  if (nsNode->CloneNode(deep,  &returnValue) == NS_OK)
    {
      dest->setNSObj(returnValue);   
      return dest;
    }
  else
    return NULL;
}

MBool Node::hasChildNodes() const
{
  PRBool returnValue;

  if (nsNode == NULL)
    return MB_FALSE;

  nsNode->HasChildNodes(&returnValue);

  return returnValue;
}

