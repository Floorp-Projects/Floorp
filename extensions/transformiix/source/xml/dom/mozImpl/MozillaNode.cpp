/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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

#include "txAtom.h"
#include "mozilladom.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"

MOZ_DECL_CTOR_COUNTER(Node)

/*
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aNode the nsIDOMNode you want to wrap
 * @param aOwner the document that owns this object
 */
Node::Node(nsIDOMNode* aNode, Document* aOwner) :
            MozillaObjectWrapper(aNode, aOwner)
{
    MOZ_COUNT_CTOR(Node);
    namespaceID=0;
}

/*
 * Destructor
 */
Node::~Node()
{
    MOZ_COUNT_DTOR(Node);
}

/*
 * Wrap a different Mozilla object with this wrapper.
 *
 * @param aNode the nsIDOMNode you want to wrap
 */
void Node::setNSObj(nsIDOMNode* aNode)
{
    NSI_FROM_TX(Node)

    // First we must remove this wrapper from the document hash table since we 
    // don't want to be associated with the existing nsIDOM* object anymore
    if (ownerDocument && nsNode)
        ownerDocument->removeWrapper(nsNode);

    // Now assume control of the new node
    MozillaObjectWrapper::setNSObj(aNode);

    // Finally, place our selves back in the hash table
    if (ownerDocument && aNode)
        ownerDocument->addWrapper(this);
}

/*
 * Call nsIDOMNode::GetNodeName to get the node's name.
 *
 * @return the node's name
 */
const String& Node::getNodeName()
{
    NSI_FROM_TX(Node)

    nodeName.clear();
    if (nsNode)
        nsNode->GetNodeName(nodeName.getNSString());
    return nodeName;
}

/*
 * Call nsIDOMNode::GetNodeValue to get the node's value.
 *
 * @return the node's name
 */
const String& Node::getNodeValue()
{
    NSI_FROM_TX(Node)

    nodeValue.clear();
    if (nsNode)
        nsNode->GetNodeValue(nodeValue.getNSString());
    return nodeValue;
}

/*
 * Call nsIDOMNode::GetNodeType to get the node's type.
 *
 * @return the node's type
 */
unsigned short Node::getNodeType() const
{
    NSI_FROM_TX(Node)
    unsigned short nodeType = 0;

    if (nsNode)
        nsNode->GetNodeType(&nodeType);
    return nodeType;
}

/*
 * Call nsIDOMNode::GetParentNode to get the node's parent.
 *
 * @return the node's parent
 */
Node* Node::getParentNode()
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNode> tmpParent;

    if (NS_SUCCEEDED(nsNode->GetParentNode(getter_AddRefs(tmpParent))))
        return ownerDocument->createWrapper(tmpParent);
    else
        return NULL;
}

/*
 * Call nsIDOMNode::GetChildNodes to get the node's childnodes.
 *
 * @return the node's children
 */
NodeList* Node::getChildNodes()
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNodeList> tmpNodeList;

    if (NS_SUCCEEDED(nsNode->GetChildNodes(getter_AddRefs(tmpNodeList))))
        return (NodeList*)ownerDocument->createNodeList(tmpNodeList);
    else
        return NULL;
}

/*
 * Call nsIDOMNode::GetFirstChild to get the node's first child.
 *
 * @return the node's first child
 */
Node* Node::getFirstChild()
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNode> tmpFirstChild;

    if (NS_SUCCEEDED(nsNode->GetFirstChild(getter_AddRefs(tmpFirstChild))))
        return ownerDocument->createWrapper(tmpFirstChild);
    else
        return NULL;
}

/*
 * Call nsIDOMNode::GetLastChild to get the node's last child.
 *
 * @return the node's first child
 */
Node* Node::getLastChild() 
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNode> tmpLastChild;

    if (NS_SUCCEEDED(nsNode->GetLastChild(getter_AddRefs(tmpLastChild))))
        return ownerDocument->createWrapper(tmpLastChild);
    else
        return NULL;
}

/*
 * Call nsIDOMNode::GetPreviousSibling to get the node's previous sibling.
 *
 * @return the node's previous sibling
 */
Node* Node::getPreviousSibling()
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNode> tmpPrevSib;

    if (NS_SUCCEEDED(nsNode->GetPreviousSibling(getter_AddRefs(tmpPrevSib))))
        return ownerDocument->createWrapper(tmpPrevSib);
    else
        return NULL;
}

/*
 * Call nsIDOMNode::GetNextSibling to get the node's next sibling.
 *
 * @return the node's next sibling
 */
Node* Node::getNextSibling()
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNode> tmpNextSib;

    if (NS_SUCCEEDED(nsNode->GetNextSibling(getter_AddRefs(tmpNextSib))))
        return ownerDocument->createWrapper(tmpNextSib);
    else
        return NULL;
}

/*
 * Call nsIDOMNode::GetAttributes to get the node's attributes.
 *
 * @return the node's attributes
 */
NamedNodeMap* Node::getAttributes()
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNamedNodeMap> tmpAttributes;

    if (NS_SUCCEEDED(nsNode->GetAttributes(getter_AddRefs(tmpAttributes))))
        return (NamedNodeMap*)ownerDocument->createNamedNodeMap(tmpAttributes);
    else
        return NULL;
}

/*
 * Get this wrapper's owning document.
 *
 * @return the wrapper's owning document
 */
Document* Node::getOwnerDocument()
{
    return ownerDocument;
}

/*
 * Call nsIDOMNode::SetNodeValue to set this node's value.
 *
 * @param aNewNodeValue the new value for the node
 */
void Node::setNodeValue(const String& aNewNodeValue)
{
    NSI_FROM_TX(Node)

    if (nsNode)
        nsNode->SetNodeValue(aNewNodeValue.getConstNSString());
}

/*
 * Call nsIDOMNode::insertBefore to insert a new child before an existing child.
 *
 * @param aNewChild the new child to insert
 * @param aRefChild the child before which the new child is inserted
 *
 * @return the inserted child
 */
Node* Node::insertBefore(Node* aNewChild, Node* aRefChild)
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNode> newChild(do_QueryInterface(GET_NSOBJ(aNewChild)));
    nsCOMPtr<nsIDOMNode> refChild(do_QueryInterface(GET_NSOBJ(aRefChild)));
    nsCOMPtr<nsIDOMNode> returnValue;

    if (NS_SUCCEEDED(nsNode->InsertBefore(newChild, refChild,
            getter_AddRefs(returnValue))))
        return ownerDocument->createWrapper(returnValue);
    else
        return NULL;
}

/*
 * Call nsIDOMNode::ReplaceChild to replace an existing child with a new child.
 *
 * @param aNewChild the new child to insert
 * @param aOldChild the child that has to be replaced
 *
 * @return the replaced child
 */
Node* Node::replaceChild(Node* aNewChild, Node* aOldChild)
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNode> newChild(do_QueryInterface(GET_NSOBJ(aNewChild)));
    nsCOMPtr<nsIDOMNode> oldChild(do_QueryInterface(GET_NSOBJ(aOldChild)));
    nsCOMPtr<nsIDOMNode> returnValue;

    if (NS_SUCCEEDED(nsNode->ReplaceChild(newChild,
               oldChild, getter_AddRefs(returnValue))))
        return (Node*)ownerDocument->removeWrapper(returnValue.get());
    else
        return NULL;
}

/*
 * Call nsIDOMNode::RemoveChild to remove a child.
 *
 * @param aOldChild the child to remove
 *
 * @return the removed child
 */
Node* Node::removeChild(Node* aOldChild)
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNode> oldChild(do_QueryInterface(GET_NSOBJ(aOldChild)));
    nsCOMPtr<nsIDOMNode> returnValue;

    if (NS_SUCCEEDED(nsNode->RemoveChild(oldChild,
            getter_AddRefs(returnValue))))
        return (Node*)ownerDocument->removeWrapper(returnValue.get());
    else
        return NULL;
}

/*
 * Call nsIDOMNode::AppendChild to append a child to the current node.
 *
 * @param aNewChild the child to append
 *
 * @return the new child
 */
Node* Node::appendChild(Node* aNewChild)
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNode> newChild(do_QueryInterface(GET_NSOBJ(aNewChild)));
    nsCOMPtr<nsIDOMNode> returnValue;

    if (NS_SUCCEEDED(nsNode->AppendChild(newChild,
            getter_AddRefs(returnValue))))
        return ownerDocument->createWrapper(returnValue);
    else
        return NULL;
}

/*
 * Call nsIDOMNode::CloneNode to clone this node.
 *
 * @param aDeep recursive cloning?
 * @param aDest the Node to put the cloned nsIDOMNode into
 *
 * @return the new (cloned) node
 */
Node* Node::cloneNode(MBool aDeep, Node* aDest)
{
    NSI_FROM_TX_NULL_CHECK(Node)
    nsCOMPtr<nsIDOMNode> returnValue;

    if (NS_SUCCEEDED(nsNode->CloneNode(aDeep, getter_AddRefs(returnValue))))
    {
        aDest->setNSObj(returnValue);  
        return aDest;
    }
    else
        return NULL;
}

/*
 * Call nsIDOMNode::HasChildNodes to return if this node has children.
 *
 * @return boolean value that says if this node has children
 */
MBool Node::hasChildNodes() const
{
    NSI_FROM_TX(Node)
    PRBool returnValue = MB_FALSE;

    if (nsNode)
        nsNode->HasChildNodes(&returnValue);
    return returnValue;
}

/*
 * Returns the Namespace URI
 * uses the Mozilla dom
 * Intoduced in DOM2
 */
String Node::getNamespaceURI()
{
    NSI_FROM_TX(Node)
    String uri;
    if (nsNode)
        nsNode->GetNamespaceURI(uri.getNSString());
    return uri;
}

/*
 * Returns the local name atomized
 *
 * @return the node's localname atom
 */
MBool Node::getLocalName(txAtom** aLocalName)
{
    if (!aLocalName)
        return MB_FALSE;
    *aLocalName = 0;
    return MB_TRUE;
}

/*
 * Returns the namespace ID of the node
 *
 * @return the node's namespace ID
 */
PRInt32 Node::getNamespaceID()
{
    return namespaceID;
}

/*
 * Returns the parent node according to the XPath datamodel
 */
Node* Node::getXPathParent()
{
    return getParentNode();
}

/*
 * Returns the namespace ID associated with the given prefix in the context
 * of this node.
 *
 * Creating our own implementation until content provides us with a way
 * to do it, that is a way that jst doesn't want to kill.
 *
 * @param prefix atom for prefix to look up
 * @return namespace ID for prefix
 */
PRInt32 Node::lookupNamespaceID(txAtom* prefix)
{
    NSI_FROM_TX(Node)
    nsresult rv;
    
    if (!nsNode)
        return kNameSpaceID_Unknown;
    if (prefix == txXMLAtoms::XMLNSPrefix)
        return kNameSpaceID_XMLNS;
    if (prefix == txXMLAtoms::XMLPrefix)
        return kNameSpaceID_XML;

    nsCOMPtr<nsIContent> elem;
    unsigned short nodeType = 0;
    nsNode->GetNodeType(&nodeType);
    switch(nodeType) {
        case Node::ATTRIBUTE_NODE :
        {
            nsCOMPtr<nsIDOMElement> owner;
            nsCOMPtr<nsIDOMAttr> attr(do_QueryInterface(nsNode));

            rv = attr->GetOwnerElement(getter_AddRefs(owner));
            NS_ENSURE_SUCCESS(rv, kNameSpaceID_Unknown);
                
            elem = do_QueryInterface(owner);
            break;
        }
        default:
            //XXX Namespace: we have to handle namespace nodes here
            elem = do_QueryInterface(nsNode);
    }

    while (elem) {
        nsAutoString uri;
        rv = elem->GetAttr(kNameSpaceID_XMLNS, prefix, uri);
        NS_ENSURE_SUCCESS(rv, kNameSpaceID_Unknown);
        if (rv != NS_CONTENT_ATTR_NOT_THERE) {
            PRInt32 nsId;
            NS_ASSERTION(ownerDocument->nsNSManager,
                         "owner document lacks namespace manager");
            if (!ownerDocument->nsNSManager)
                return kNameSpaceID_Unknown;
            ownerDocument->nsNSManager->RegisterNameSpace(uri, nsId);
            return nsId;
        }

        nsCOMPtr<nsIContent> temp(elem);
        rv = temp->GetParent(*getter_AddRefs(elem));
        NS_ENSURE_SUCCESS(rv, kNameSpaceID_Unknown);
    }
    // Error, namespace not found
    return kNameSpaceID_Unknown;
}

/*
 * Returns the base URI of the node. Acccounts for xml:base
 * attributes.
 *
 * @return base URI for the node
 */
String Node::getBaseURI()
{
    NSI_FROM_TX(Node)
    nsCOMPtr<nsIDOM3Node> nsDOM3Node = do_QueryInterface(nsNode);
    String url;

    if (nsDOM3Node)
        nsDOM3Node->GetBaseURI(url.getNSString());

    return url;
}
