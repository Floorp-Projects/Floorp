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
 * Contributor(s): Tom Kneeland
 *                 Peter Van der Beken <peter.vanderbeken@pandora.be>
 *
 */

/* Implementation of the wrapper class to convert the Mozilla nsIDOMNode
   interface into a TransforMIIX Node interface.
*/

#include "mozilladom.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aNode the nsIDOMNode you want to wrap
 * @param aOwner the document that owns this object
 */
Node::Node(nsIDOMNode* aNode, Document* aOwner) :
            MozillaObjectWrapper(aNode, aOwner)
{
    nsNode = aNode;
}

/**
 * Destructor
 */
Node::~Node()
{
}

/**
 * Wrap a different Mozilla object with this wrapper.
 *
 * @param aNode the nsIDOMNode you want to wrap
 */
void Node::setNSObj(nsIDOMNode* aNode)
{
    // First we must remove this wrapper from the document hash table since we 
    // don't want to be associated with the existing nsIDOM* object anymore
    if (ownerDocument && nsNode)
        ownerDocument->removeWrapper(getKey());

    // Now assume control of the new node
    MozillaObjectWrapper::setNSObj(aNode);
    nsNode = aNode;

    // Finally, place our selves back in the hash table
    if (ownerDocument && aNode)
        ownerDocument->addWrapper(this, getKey());
}

/**
 * Wrap a different Mozilla object with this wrapper and set document owner.
 *
 * @param aNode the nsIDOMNode you want to wrap
 */
void Node::setNSObj(nsIDOMNode* aNode, Document* aOwner)
{
    MozillaObjectWrapper::setNSObj(aNode, aOwner);
    nsNode = aNode;
}

/**
 * Get the Mozilla object wrapped with this wrapper.
 *
 * @return the Mozilla object wrapped with this wrapper
 */
nsIDOMNode* Node::getNSObj()
{
    return nsNode;
}

/**
 * Call nsIDOMNode::GetNodeName to get the node's name.
 *
 * @return the node's name
 */
const String& Node::getNodeName()
{
    nodeName.clear();
    nsNode->GetNodeName(nodeName.getNSString());
    return nodeName;
}

/**
 * Call nsIDOMNode::GetNodeValue to get the node's value.
 *
 * @return the node's name
 */
const String& Node::getNodeValue()
{
    nodeValue.clear();
    nsNode->GetNodeValue(nodeValue.getNSString());
    return nodeValue;
}

/**
 * Call nsIDOMNode::GetNodeType to get the node's type.
 *
 * @return the node's type
 */
unsigned short Node::getNodeType() const
{
    unsigned short nodeType;

    if (nsNode == NULL)
        return 0;

    nsNode->GetNodeType(&nodeType);
    return nodeType;
}

/**
 * Call nsIDOMNode::GetParentNode to get the node's parent.
 *
 * @return the node's parent
 */
Node* Node::getParentNode()
{
    nsCOMPtr<nsIDOMNode> tmpParent;

    if (nsNode == NULL)
        return NULL;

    if (NS_SUCCEEDED(nsNode->GetParentNode(getter_AddRefs(tmpParent))))
        return ownerDocument->createWrapper(tmpParent);
    else
        return NULL;
}

/**
 * Call nsIDOMNode::GetChildNodes to get the node's childnodes.
 *
 * @return the node's children
 */
NodeList* Node::getChildNodes()
{
    nsCOMPtr<nsIDOMNodeList> tmpNodeList;

    if (nsNode == NULL)
        return NULL;

    if (NS_SUCCEEDED(nsNode->GetChildNodes(getter_AddRefs(tmpNodeList))))
        return (NodeList*)ownerDocument->createNodeList(tmpNodeList);
    else
        return NULL;
}

/**
 * Call nsIDOMNode::GetFirstChild to get the node's first child.
 *
 * @return the node's first child
 */
Node* Node::getFirstChild()
{
    nsCOMPtr<nsIDOMNode> tmpFirstChild;

    if (nsNode == NULL)
        return NULL;

    if (NS_SUCCEEDED(nsNode->GetFirstChild(getter_AddRefs(tmpFirstChild))))
        return ownerDocument->createWrapper(tmpFirstChild);
    else
        return NULL;
}

/**
 * Call nsIDOMNode::GetLastChild to get the node's last child.
 *
 * @return the node's first child
 */
Node* Node::getLastChild() 
{
  nsCOMPtr<nsIDOMNode> tmpLastChild;

  if (nsNode == NULL)
    return NULL;

  if (NS_SUCCEEDED(nsNode->GetLastChild(getter_AddRefs(tmpLastChild))))
    return ownerDocument->createWrapper(tmpLastChild);
  else
    return NULL;
}

/**
 * Call nsIDOMNode::GetPreviousSibling to get the node's previous sibling.
 *
 * @return the node's previous sibling
 */
Node* Node::getPreviousSibling()
{
    nsCOMPtr<nsIDOMNode> tmpPrevSib;

    if (nsNode == NULL)
        return NULL;

    if (NS_SUCCEEDED(nsNode->GetPreviousSibling(getter_AddRefs(tmpPrevSib))))
        return ownerDocument->createWrapper(tmpPrevSib);
    else
        return NULL;
}

/**
 * Call nsIDOMNode::GetNextSibling to get the node's next sibling.
 *
 * @return the node's next sibling
 */
Node* Node::getNextSibling()
{
    nsCOMPtr<nsIDOMNode> tmpNextSib;

    if (nsNode == NULL)
        return NULL;

    if (NS_SUCCEEDED(nsNode->GetNextSibling(getter_AddRefs(tmpNextSib))))
        return ownerDocument->createWrapper(tmpNextSib);
    else
        return NULL;
}

/**
 * Call nsIDOMNode::GetAttributes to get the node's attributes.
 *
 * @return the node's attributes
 */
NamedNodeMap* Node::getAttributes()
{
    nsCOMPtr<nsIDOMNamedNodeMap> tmpAttributes;

    if (nsNode == NULL)
        return NULL;

    if (NS_SUCCEEDED(nsNode->GetAttributes(getter_AddRefs(tmpAttributes))))
        return (NamedNodeMap*)ownerDocument->createNamedNodeMap(tmpAttributes);
    else
        return NULL;
}

/**
 * Get this wrapper's owning document.
 *
 * @return the wrapper's owning document
 */
Document* Node::getOwnerDocument()
{
    return ownerDocument;
}

/**
 * Call nsIDOMNode::SetNodeValue to set this node's value.
 *
 * @param aNewNodeValue the new value for the node
 */
void Node::setNodeValue(const String& aNewNodeValue)
{
    if (nsNode != NULL)
        nsNode->SetNodeValue(aNewNodeValue.getConstNSString());
}

/**
 * Call nsIDOMNode::insertBefore to insert a new child before an existing child.
 *
 * @param aNewChild the new child to insert
 * @param aRefChild the child before which the new child is inserted
 *
 * @return the inserted child
 */
Node* Node::insertBefore(Node* aNewChild, Node* aRefChild)
{
    nsCOMPtr<nsIDOMNode> returnValue;

    if (nsNode == NULL)
        return NULL;

    if (NS_SUCCEEDED(nsNode->InsertBefore(aNewChild->getNSObj(),
            aRefChild->getNSObj(), getter_AddRefs(returnValue))))
        return ownerDocument->createWrapper(returnValue);
    else
        return NULL;
}

/**
 * Call nsIDOMNode::ReplaceChild to replace an existing child with a new child.
 *
 * @param aNewChild the new child to insert
 * @param aOldChild the child that has to be replaced
 *
 * @return the replaced child
 */
Node* Node::replaceChild(Node* aNewChild, Node* aOldChild)
{
    nsCOMPtr<nsIDOMNode> returnValue;

    if (nsNode == NULL)
        return NULL;

    if (NS_SUCCEEDED(nsNode->ReplaceChild(aNewChild->getNSObj(),
               aOldChild->getNSObj(), getter_AddRefs(returnValue))))
        return (Node*)ownerDocument->removeWrapper(returnValue.get());
    else
        return NULL;
}

/**
 * Call nsIDOMNode::RemoveChild to remove a child.
 *
 * @param aOldChild the child to remove
 *
 * @return the removed child
 */
Node* Node::removeChild(Node* aOldChild)
{
    nsCOMPtr<nsIDOMNode> returnValue;

    if (nsNode == NULL)
        return NULL;

    if (NS_SUCCEEDED(nsNode->RemoveChild(aOldChild->getNSObj(),
            getter_AddRefs(returnValue))))
        return (Node*)ownerDocument->removeWrapper(returnValue.get());
    else
        return NULL;
}

/**
 * Call nsIDOMNode::AppendChild to append a child to the current node.
 *
 * @param aNewChild the child to append
 *
 * @return the new child
 */
Node* Node::appendChild(Node* aNewChild)
{
    nsCOMPtr<nsIDOMNode> returnValue;

    if (nsNode == NULL)
        return NULL;

    if (NS_SUCCEEDED(nsNode->AppendChild(aNewChild->getNSObj(),
            getter_AddRefs(returnValue))))
        return ownerDocument->createWrapper(returnValue);
    else
        return NULL;
}

/**
 * Call nsIDOMNode::CloneNode to clone this node.
 *
 * @param aDeep recursive cloning?
 * @param aDest the Node to put the cloned nsIDOMNode into
 *
 * @return the new (cloned) node
 */
Node* Node::cloneNode(MBool aDeep, Node* aDest)
{
    nsCOMPtr<nsIDOMNode> returnValue;

    if (nsNode == NULL)
        return NULL;

    if (NS_SUCCEEDED(nsNode->CloneNode(aDeep, getter_AddRefs(returnValue))))
    {
        aDest->setNSObj(returnValue);  
        return aDest;
    }
    else
        return NULL;
}

/**
 * Call nsIDOMNode::HasChildNodes to return if this node has children.
 *
 * @return boolean value that says if this node has children
 */
MBool Node::hasChildNodes() const
{
    PRBool returnValue;

    if (nsNode == NULL)
        return MB_FALSE;

    nsNode->HasChildNodes(&returnValue);
    return returnValue;
}
