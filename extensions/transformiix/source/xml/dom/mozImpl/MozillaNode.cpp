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

/**
 * Implementation of the wrapper class to convert the Mozilla nsIDOMNode
 * interface into a TransforMIIX Node interface.
 */

#include "mozilladom.h"
#include "nsIDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMElement.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "txAtoms.h"

MOZ_DECL_CTOR_COUNTER(Node)

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aNode the nsIDOMNode you want to wrap
 * @param aOwner the document that owns this object
 */
Node::Node(nsIDOMNode* aNode, Document* aOwner) :
    MozillaObjectWrapper(aNode, aOwner),
    mNamespaceID(0),
    mOrderInfo(0)
{
    MOZ_COUNT_CTOR(Node);
}

/**
 * Destructor
 */
Node::~Node()
{
    MOZ_COUNT_DTOR(Node);
    delete mOrderInfo;
}

/**
 * Call nsIDOMNode::GetNodeName to get the node's name.
 *
 * @return the node's name
 */
const String& Node::getNodeName()
{
    NSI_FROM_TX(Node);
    nsNode->GetNodeName(mNodeName);
    return mNodeName;
}

/**
 * Call nsIDOMNode::GetNodeValue to get the node's value.
 *
 * @return the node's name
 */
const String& Node::getNodeValue()
{
    NSI_FROM_TX(Node);
    nsNode->GetNodeValue(mNodeValue);
    return mNodeValue;
}

/**
 * Call nsIDOMNode::GetNodeType to get the node's type.
 *
 * @return the node's type
 */
unsigned short Node::getNodeType() const
{
    NSI_FROM_TX(Node);
    unsigned short nodeType = 0;
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
    NSI_FROM_TX(Node);
    nsCOMPtr<nsIDOMNode> tmpParent;
    nsNode->GetParentNode(getter_AddRefs(tmpParent));
    if (!tmpParent) {
        return nsnull;
    }
    return mOwnerDocument->createWrapper(tmpParent);
}

/**
 * Call nsIDOMNode::GetChildNodes to get the node's childnodes.
 *
 * @return the node's children
 */
NodeList* Node::getChildNodes()
{
    NSI_FROM_TX(Node);
    nsCOMPtr<nsIDOMNodeList> tmpNodeList;
    nsNode->GetChildNodes(getter_AddRefs(tmpNodeList));
    if (!tmpNodeList) {
        return nsnull;
    }
    return mOwnerDocument->createNodeList(tmpNodeList);
}

/**
 * Call nsIDOMNode::GetFirstChild to get the node's first child.
 *
 * @return the node's first child
 */
Node* Node::getFirstChild()
{
    NSI_FROM_TX(Node);
    nsCOMPtr<nsIDOMNode> tmpFirstChild;
    nsNode->GetFirstChild(getter_AddRefs(tmpFirstChild));
    if (!tmpFirstChild) {
        return nsnull;
    }
    return mOwnerDocument->createWrapper(tmpFirstChild);
}

/**
 * Call nsIDOMNode::GetLastChild to get the node's last child.
 *
 * @return the node's first child
 */
Node* Node::getLastChild() 
{
    NSI_FROM_TX(Node);
    nsCOMPtr<nsIDOMNode> tmpLastChild;
    nsNode->GetLastChild(getter_AddRefs(tmpLastChild));
    if (!tmpLastChild) {
        return nsnull;
    }
    return mOwnerDocument->createWrapper(tmpLastChild);
}

/**
 * Call nsIDOMNode::GetPreviousSibling to get the node's previous sibling.
 *
 * @return the node's previous sibling
 */
Node* Node::getPreviousSibling()
{
    NSI_FROM_TX(Node);
    nsCOMPtr<nsIDOMNode> tmpPrevSib;
    nsNode->GetPreviousSibling(getter_AddRefs(tmpPrevSib));
    if (!tmpPrevSib) {
        return nsnull;
    }
    return mOwnerDocument->createWrapper(tmpPrevSib);
}

/**
 * Call nsIDOMNode::GetNextSibling to get the node's next sibling.
 *
 * @return the node's next sibling
 */
Node* Node::getNextSibling()
{
    NSI_FROM_TX(Node);
    nsCOMPtr<nsIDOMNode> tmpNextSib;
    nsNode->GetNextSibling(getter_AddRefs(tmpNextSib));
    if (!tmpNextSib) {
        return nsnull;
    }
    return mOwnerDocument->createWrapper(tmpNextSib);
}

/**
 * Call nsIDOMNode::GetAttributes to get the node's attributes.
 *
 * @return the node's attributes
 */
NamedNodeMap* Node::getAttributes()
{
    NSI_FROM_TX(Node);
    nsCOMPtr<nsIDOMNamedNodeMap> tmpAttributes;
    nsNode->GetAttributes(getter_AddRefs(tmpAttributes));
    if (!tmpAttributes) {
        return nsnull;
    }
    return mOwnerDocument->createNamedNodeMap(tmpAttributes);
}

/**
 * Get this wrapper's owning document.
 *
 * @return the wrapper's owning document
 */
Document* Node::getOwnerDocument()
{
    return mOwnerDocument;
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
    if (!aNewChild) {
        return nsnull;
    }
    NSI_FROM_TX(Node);
    nsCOMPtr<nsIDOMNode> newChild(do_QueryInterface(aNewChild->getNSObj()));
    nsCOMPtr<nsIDOMNode> returnValue;
    nsNode->AppendChild(newChild, getter_AddRefs(returnValue));
    if (!returnValue) {
        return nsnull;
    }
    return mOwnerDocument->createWrapper(returnValue);
}

/**
 * Call nsIDOMNode::HasChildNodes to return if this node has children.
 *
 * @return boolean value that says if this node has children
 */
MBool Node::hasChildNodes() const
{
    NSI_FROM_TX(Node);
    PRBool returnValue = MB_FALSE;
    nsNode->HasChildNodes(&returnValue);
    return returnValue;
}

/**
 * Returns the Namespace URI
 * uses the Mozilla dom
 * Intoduced in DOM2
 */
String Node::getNamespaceURI()
{
    NSI_FROM_TX(Node);
    String uri;
    nsNode->GetNamespaceURI(uri);
    return uri;
}

/**
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

/**
 * Returns the namespace ID of the node
 *
 * @return the node's namespace ID
 */
PRInt32 Node::getNamespaceID()
{
    return mNamespaceID;
}

/**
 * Returns the parent node according to the XPath datamodel
 */
Node* Node::getXPathParent()
{
    return getParentNode();
}

/**
 * Returns the namespace ID associated with the given prefix in the context
 * of this node.
 *
 * Creating our own implementation until content provides us with a way
 * to do it, that is a way that jst doesn't want to kill.
 *
 * @param prefix atom for prefix to look up
 * @return namespace ID for prefix
 */
PRInt32 Node::lookupNamespaceID(txAtom* aPrefix)
{
    NSI_FROM_TX(Node);
    nsresult rv;
    
    if (aPrefix == txXMLAtoms::xmlns) {
        return kNameSpaceID_XMLNS;
    }
    if (aPrefix == txXMLAtoms::xml) {
        return kNameSpaceID_XML;
    }

    nsCOMPtr<nsIContent> elem;
    unsigned short nodeType = 0;
    nsNode->GetNodeType(&nodeType);
    switch(nodeType) {
        case Node::ATTRIBUTE_NODE:
        {
            nsCOMPtr<nsIDOMElement> owner;
            nsCOMPtr<nsIDOMAttr> attr(do_QueryInterface(nsNode));

            rv = attr->GetOwnerElement(getter_AddRefs(owner));
            NS_ENSURE_SUCCESS(rv, kNameSpaceID_Unknown);
                
            elem = do_QueryInterface(owner);
            break;
        }
        default:
        {
            //XXX Namespace: we have to handle namespace nodes here
            elem = do_QueryInterface(nsNode);
        }
    }

    if (!aPrefix || aPrefix == txXMLAtoms::_empty) {
        aPrefix = txXMLAtoms::xmlns;
    }

    while (elem) {
        nsAutoString uri;
        rv = elem->GetAttr(kNameSpaceID_XMLNS, aPrefix, uri);
        NS_ENSURE_SUCCESS(rv, kNameSpaceID_Unknown);
        if (rv != NS_CONTENT_ATTR_NOT_THERE) {
            PRInt32 nsId;
            NS_ASSERTION(gTxNameSpaceManager, "No namespace manager");
            if (!gTxNameSpaceManager) {
                return kNameSpaceID_Unknown;
            }
            gTxNameSpaceManager->RegisterNameSpace(uri, nsId);
            return nsId;
        }

        nsCOMPtr<nsIContent> temp(elem);
        rv = temp->GetParent(*getter_AddRefs(elem));
        NS_ENSURE_SUCCESS(rv, kNameSpaceID_Unknown);
    }

    if (aPrefix == txXMLAtoms::xmlns) {
        // No default namespace
        return kNameSpaceID_None;
    }

    // Error, namespace not found
    return kNameSpaceID_Unknown;
}

/**
 * Returns the base URI of the node. Acccounts for xml:base
 * attributes.
 *
 * @return base URI for the node
 */
String Node::getBaseURI()
{
    NSI_FROM_TX(Node);
    nsCOMPtr<nsIDOM3Node> nsDOM3Node = do_QueryInterface(nsNode);
    String url;
    if (nsDOM3Node) {
        nsDOM3Node->GetBaseURI(url);
    }
    return url;
}

/**
 * Compares document position of this node relative to another node
 */
PRInt32 Node::compareDocumentPosition(Node* aOther)
{
  OrderInfo* myOrder = getOrderInfo();
  OrderInfo* otherOrder = aOther->getOrderInfo();
  if (!myOrder || !otherOrder) {
      return -1;
  }

  if (myOrder->mRoot == otherOrder->mRoot) {
    int c = 0;
    while (c < myOrder->mSize && c < otherOrder->mSize) {
      if (myOrder->mOrder[c] < otherOrder->mOrder[c]) {
        return -1;
      }
      if (myOrder->mOrder[c] > otherOrder->mOrder[c]) {
        return 1;
      }
      ++c;
    }
    if (c < myOrder->mSize) {
      return 1;
    }
    if (c < otherOrder->mSize) {
      return -1;
    }
    return 0;
  }

  if (myOrder->mRoot < otherOrder->mRoot) {
    return -1;
  }

  return 1;
}

/**
 * Get order information for node
 */
Node::OrderInfo* Node::getOrderInfo()
{
    if (mOrderInfo) {
        return mOrderInfo;
    }

    mOrderInfo = new OrderInfo;
    if (!mOrderInfo) {
        return 0;
    }

    Node* parent = getXPathParent();
    if (!parent) {
        mOrderInfo->mOrder = 0;
        mOrderInfo->mSize = 0;
        mOrderInfo->mRoot = this;
        return mOrderInfo;
    }

    OrderInfo* parentOrder = parent->getOrderInfo();
    mOrderInfo->mSize = parentOrder->mSize + 1;
    mOrderInfo->mRoot = parentOrder->mRoot;
    mOrderInfo->mOrder = new PRUint32[mOrderInfo->mSize];
    if (!mOrderInfo->mOrder) {
        delete mOrderInfo;
        mOrderInfo = 0;
        return 0;
    }
    memcpy(mOrderInfo->mOrder,
           parentOrder->mOrder,
           parentOrder->mSize * sizeof(PRUint32*));

    int lastElem = parentOrder->mSize;
    switch(getNodeType()) {
        case Node::ATTRIBUTE_NODE:
        {
            nsCOMPtr<nsIAtom> thisName;
            getLocalName(getter_AddRefs(thisName));
            PRInt32 thisNS = getNamespaceID();

            // find this attribute in parents attrlist
            PRInt32 count, i;
            nsCOMPtr<nsIContent> owner = do_QueryInterface(parent->getNSObj());
            owner->GetAttrCount(count);
            for (i = 0; i < count; ++i) {
                nsCOMPtr<nsIAtom> attName;
                nsCOMPtr<nsIAtom> attPrefix;
                PRInt32 attNS;
                owner->GetAttrNameAt(i,
                                     attNS,
                                     *getter_AddRefs(attName),
                                     *getter_AddRefs(attPrefix));

                if (attName == thisName && attNS == thisNS) {
                    mOrderInfo->mOrder[lastElem] = i + kTxAttrIndexOffset;
                    return mOrderInfo;
                }
            }
            break;
        }
        // XXX Namespace: need to handle namespace nodes here
        default:
        {
            nsCOMPtr<nsIContent> cont(do_QueryInterface(mMozObject));

            nsISupports* parentObj = parent->getNSObj();

            nsCOMPtr<nsIContent> parentCont(do_QueryInterface(parentObj));
            // Is parent an nsIContent
            if (parentCont) {
                PRInt32 index;
                parentCont->IndexOf(cont, index);
                mOrderInfo->mOrder[lastElem] = index + kTxChildIndexOffset;
                return mOrderInfo;
            }

            nsCOMPtr<nsIDocument> parentDoc(do_QueryInterface(parentObj));
            // Is parent an nsIDocument
            if (parentDoc) {
                PRInt32 index;
                parentDoc->IndexOf(cont, index);
                mOrderInfo->mOrder[lastElem] = index + kTxChildIndexOffset;
                return mOrderInfo;
            }

            // Parent is something else, probably an attribute
            // This will happen next to never, only for expressions like
            // foo/@bar/text()
            PRUint32 i, len;
            nsCOMPtr<nsIDOMNodeList> childNodes;

            nsCOMPtr<nsIDOMNode> parentNode = do_QueryInterface(parentObj);
            nsCOMPtr<nsIDOMNode> thisNode = do_QueryInterface(mMozObject);
            parentNode->GetChildNodes(getter_AddRefs(childNodes));
            NS_ENSURE_TRUE(childNodes, 0);

            childNodes->GetLength(&len);
            for (i = 0; i < len; ++i) {
                nsCOMPtr<nsIDOMNode> node;
                childNodes->Item(i, getter_AddRefs(node));

                if (node == thisNode) {
                    mOrderInfo->mOrder[lastElem] = i + kTxChildIndexOffset;
                    return mOrderInfo;
                }
            }
            break;
        }
    }

    NS_ERROR("unable to get childindex for node");
    mOrderInfo->mOrder[lastElem] = 0;
    return mOrderInfo;
}

/**
 * OrderInfo destructor
 */
Node::OrderInfo::~OrderInfo()
{
    delete [] mOrder;
}
