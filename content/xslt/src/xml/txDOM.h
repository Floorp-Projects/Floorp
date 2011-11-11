/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Tom Kneeland (3/29/99)
//
//  Implementation of the Document Object Model Level 1 Core
//
// Modification History:
// Who  When        What
// TK   03/29/99    Created
// LF   08/06/1999  Changed static const short NodeType to enum
//                  Added "friend NamedNodeMap"; to NodeListDefinition
//

#ifndef MITRE_DOM
#define MITRE_DOM

#ifdef __BORLANDC__
#include <stdlib.h>
#endif

#include "txList.h"
#include "nsIAtom.h"
#include "nsDoubleHashtable.h"
#include "nsString.h"
#include "txCore.h"
#include "nsAutoPtr.h"

#define kTxNsNodeIndexOffset 0x00000000;
#define kTxAttrIndexOffset 0x40000000;
#define kTxChildIndexOffset 0x80000000;

class NamedNodeMap;
class Document;
class Element;
class Attr;
class ProcessingInstruction;

#define kNameSpaceID_Unknown -1
#define kNameSpaceID_None     0
// not really a namespace, but it needs to play the game
#define kNameSpaceID_XMLNS    1 
#define kNameSpaceID_XML      2
// kNameSpaceID_XSLT is 6 for module, see nsINameSpaceManager.h
#define kNameSpaceID_XSLT     3

//
// Abstract Class defining the interface for a Node.  See NodeDefinition below
// for the actual implementation of the WC3 node.
//
class Node : public TxObject
{
  public:
    //Node type constants
    //-- LF - changed to enum
    enum NodeType {
        ELEMENT_NODE = 1,
        ATTRIBUTE_NODE,
        TEXT_NODE,
        CDATA_SECTION_NODE,
        ENTITY_REFERENCE_NODE,
        ENTITY_NODE,
        PROCESSING_INSTRUCTION_NODE,
        COMMENT_NODE,
        DOCUMENT_NODE,
        DOCUMENT_TYPE_NODE,
        DOCUMENT_FRAGMENT_NODE,
        NOTATION_NODE
    };

    //Read functions
    virtual nsresult getNodeName(nsAString& aName) const = 0;
    virtual nsresult getNodeValue(nsAString& aValue) = 0;
    virtual unsigned short getNodeType() const = 0;
    virtual Node* getParentNode() const = 0;
    virtual Node* getFirstChild() const = 0;
    virtual Node* getLastChild() const = 0;
    virtual Node* getPreviousSibling() const = 0;
    virtual Node* getNextSibling() const = 0;
    virtual Document* getOwnerDocument() const = 0;

    //Node manipulation functions
    virtual Node* appendChild(Node* newChild) = 0;

    virtual bool hasChildNodes() const = 0;
    
    //From DOM3 26-Jan-2001 WD
    virtual nsresult getBaseURI(nsAString& aURI) = 0;

    //Introduced in DOM2
    virtual nsresult getNamespaceURI(nsAString& aNSURI) = 0;

    //txXPathNode functions
    virtual bool getLocalName(nsIAtom** aLocalName) = 0;
    virtual PRInt32 getNamespaceID() = 0;
    virtual Node* getXPathParent() = 0;
    virtual PRInt32 compareDocumentPosition(Node* aOther) = 0;
};

//
// Definition and Implementation of Node and NodeList functionality.  This is
// the central class, from which all other DOM classes (objects) are derrived.
// Users of this DOM should work strictly with the Node interface and NodeList
// interface (see above for those definitions)
//
class NodeDefinition : public Node
{
  public:
    virtual ~NodeDefinition();      //Destructor, delete all children of node

    //Read functions
    virtual nsresult getNodeName(nsAString& aName) const;
    nsresult getNodeValue(nsAString& aValue);
    unsigned short getNodeType() const;
    Node* getParentNode() const;
    Node* getFirstChild() const;
    Node* getLastChild() const;
    Node* getPreviousSibling() const;
    Node* getNextSibling() const;
    Document* getOwnerDocument() const;

    //Child node manipulation functions
    virtual Node* appendChild(Node* newChild);

    bool hasChildNodes() const;
    
    //From DOM3 26-Jan-2001 WD
    virtual nsresult getBaseURI(nsAString& aURI);

    //Introduced in DOM2
    nsresult getNamespaceURI(nsAString& aNSURI);

    //txXPathNode functions
    virtual bool getLocalName(nsIAtom** aLocalName);
    virtual PRInt32 getNamespaceID();
    virtual Node* getXPathParent();
    virtual PRInt32 compareDocumentPosition(Node* aOther);

    //Only to be used from XMLParser
    void appendData(const PRUnichar* aData, int aLength)
    {
      nodeValue.Append(aData, aLength);
    };

  protected:
    friend class Document;
    friend class txXPathTreeWalker;
    friend class txXPathNodeUtils;
    NodeDefinition(NodeType type, nsIAtom *aLocalName,
                   const nsAString& value, Document* owner);

    //Name, value, and attributes for this node.  Available to derrived
    //classes, since those derrived classes have a better idea how to use them,
    //than the generic node does.
    nsCOMPtr<nsIAtom> mLocalName;
    nsString nodeValue;

    NodeDefinition* implAppendChild(NodeDefinition* newChild);
    NodeDefinition* implRemoveChild(NodeDefinition* oldChild);

  private:
    //Type of node this is
    NodeType nodeType;

    //Data members for linking this Node to its parent and siblings
    NodeDefinition* parentNode;
    NodeDefinition* previousSibling;
    NodeDefinition* nextSibling;

    //Pointer to the node's document
    Document* ownerDocument;

    PRUint32 length;

    //Data members for maintaining a list of child nodes
    NodeDefinition* firstChild;
    NodeDefinition* lastChild;

    // Struct to hold document order information
    struct OrderInfo {
        ~OrderInfo();
        PRUint32* mOrder;
        PRInt32 mSize;
        Node* mRoot;
    };

    // OrderInfo object for comparing document order
    OrderInfo* mOrderInfo;

    // Helperfunction for compareDocumentOrder
    OrderInfo* getOrderInfo();
};

//
//Definition and Implementation of a Document.
//

/**
 * nsDoubleHashtable definitions for IDs
 *
 * It may be possible to share the key value with the element,
 * but that may leave entries without keys, as the entries
 * are constructed from the key value and the setting of mElement
 * happens late. As pldhash.h ain't clear on this, we store the
 * key by inheriting from PLDHashStringEntry.
 */
class txIDEntry : public PLDHashStringEntry
{
public:
    txIDEntry(const void* aKey) : PLDHashStringEntry(aKey), mElement(nsnull)
    {
    }
    Element* mElement;
};
DECL_DHASH_WRAPPER(txIDMap, txIDEntry, nsAString&)

class Document : public NodeDefinition
{
  public:
    Document();

    Element* getDocumentElement();

    //Factory functions for various node types
    Node* createComment(const nsAString& aData);
    ProcessingInstruction* createProcessingInstruction(nsIAtom *aTarget,
                                                       const nsAString& aData);
    Node* createTextNode(const nsAString& theData);

    Element* createElementNS(nsIAtom *aPrefix, nsIAtom *aLocalName,
                             PRInt32 aNamespaceID);
    Element* getElementById(const nsAString& aID);

    // Node manipulation functions
    Node* appendChild(Node* newChild);

    //Override to return documentBaseURI
    nsresult getBaseURI(nsAString& aURI);

  private:
    bool setElementID(const nsAString& aID, Element* aElement);

    Element* documentElement;

    // This class is friend to be able to set the documentBaseURI
    // and IDs.
    friend class txXMLParser;
    txIDMap mIDMap;
    nsString documentBaseURI;
};

//
//Definition and Implementation of an Element
//
class Element : public NodeDefinition
{
  public:
    NamedNodeMap* getAttributes();

    nsresult appendAttributeNS(nsIAtom *aPrefix, nsIAtom *aLocalName,
                               PRInt32 aNamespaceID, const nsAString& aValue);

    // Node manipulation functions
    Node* appendChild(Node* newChild);

    //txXPathNode functions override
    nsresult getNodeName(nsAString& aName) const;
    bool getLocalName(nsIAtom** aLocalName);
    PRInt32 getNamespaceID();
    bool getAttr(nsIAtom* aLocalName, PRInt32 aNSID, nsAString& aValue);
    bool hasAttr(nsIAtom* aLocalName, PRInt32 aNSID);

    // ID getter
    bool getIDValue(nsAString& aValue);

    Attr *getFirstAttribute()
    {
      return mFirstAttribute;
    }

  private:
    friend class Document;
    void setIDValue(const nsAString& aValue);

    Element(nsIAtom *aPrefix, nsIAtom *aLocalName, PRInt32 aNamespaceID,
            Document* aOwner);

    nsAutoPtr<Attr> mFirstAttribute;
    nsString mIDValue;
    nsCOMPtr<nsIAtom> mPrefix;
    PRInt32 mNamespaceID;
};

//
//Definition and Implementation of a Attr
//    NOTE:  For the time bing use just the default functionality found in the
//           NodeDefinition class
//
class Attr : public NodeDefinition
{
  public:
    Node* appendChild(Node* newChild);

    //txXPathNode functions override
    nsresult getNodeName(nsAString& aName) const;
    bool getLocalName(nsIAtom** aLocalName);
    PRInt32 getNamespaceID();
    Node* getXPathParent();
    bool equals(nsIAtom *aLocalName, PRInt32 aNamespaceID)
    {
      return mLocalName == aLocalName && aNamespaceID == mNamespaceID;
    }
    Attr *getNextAttribute()
    {
      return mNextAttribute;
    }

  private:
    friend class Element;

    Attr(nsIAtom *aPrefix, nsIAtom *aLocalName, PRInt32 aNamespaceID,
         Element *aOwnerElement, const nsAString &aValue);

    nsCOMPtr<nsIAtom> mPrefix;
    PRInt32 mNamespaceID;
    Element *mOwnerElement;
    nsAutoPtr<Attr> mNextAttribute;
};

//
//Definition and Implemention of a ProcessingInstruction node.  Most
//functionality is inherrited from NodeDefinition.
//  The Target of a processing instruction is stored in the nodeName datamember
//  inherrited from NodeDefinition.
//  The Data of a processing instruction is stored in the nodeValue datamember
//  inherrited from NodeDefinition
//
class ProcessingInstruction : public NodeDefinition
{
  public:
    //txXPathNode functions override
    bool getLocalName(nsIAtom** aLocalName);

  private:
    friend class Document;
    ProcessingInstruction(nsIAtom *theTarget, const nsAString& theData,
                          Document* owner);
};

class txStandaloneNamespaceManager
{
public:
    static PRInt32 getNamespaceID(const nsAString& aURI)
    {
        if (!mNamespaces && !init())
            return kNameSpaceID_Unknown;

        PRInt32 id = mNamespaces->IndexOf(aURI);
        if (id != -1) {
            return id + 1;
        }

        if (!mNamespaces->AppendString(aURI)) {
            NS_ERROR("Out of memory, namespaces are getting lost");
            return kNameSpaceID_Unknown;
        }

        return mNamespaces->Count();
    }

    static nsresult getNamespaceURI(const PRInt32 aID, nsAString& aNSURI)
    {
        // empty namespace, and errors
        aNSURI.Truncate();
        if (aID <= 0 || (!mNamespaces && !init()) ||
            aID > mNamespaces->Count()) {
            return NS_OK;
        }

        aNSURI = *mNamespaces->StringAt(aID - 1);
        return NS_OK;
    }

    static bool init()
    {
        NS_ASSERTION(!mNamespaces,
                     "called without matching shutdown()");
        if (mNamespaces)
            return true;
        mNamespaces = new nsStringArray();
        if (!mNamespaces)
            return false;
        /*
         * Hardwiring some Namespace IDs.
         * no Namespace is 0
         * xmlns prefix is 1, mapped to http://www.w3.org/2000/xmlns/
         * xml prefix is 2, mapped to http://www.w3.org/XML/1998/namespace
         */
        if (!mNamespaces->AppendString(NS_LITERAL_STRING("http://www.w3.org/2000/xmlns/")) ||
            !mNamespaces->AppendString(NS_LITERAL_STRING("http://www.w3.org/XML/1998/namespace")) ||
            !mNamespaces->AppendString(NS_LITERAL_STRING("http://www.w3.org/1999/XSL/Transform"))) {
            delete mNamespaces;
            mNamespaces = 0;
            return false;
        }

        return true;
    }

    static void shutdown()
    {
        NS_ASSERTION(mNamespaces, "called without matching init()");
        if (!mNamespaces)
            return;
        delete mNamespaces;
        mNamespaces = nsnull;
    }

private:
    static nsStringArray* mNamespaces;
};

#define TX_IMPL_DOM_STATICS \
    nsStringArray* txStandaloneNamespaceManager::mNamespaces = 0

#endif
