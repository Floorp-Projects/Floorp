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
 */

/**
 * Definition of the wrapper classes.
 */

/**
 * Implementation of the Mozilla Wrapper classes for the Mozilla DOM to
 * TransforMiiX DOM interface conversion. Note that these wrapper classes are
 * only interchangable with the TransforMiiX DOM at compile time. They are
 * merely a copy of the TransforMiiX interface/class structure wich then
 * deffer their processing to a Mozilla object. Complete interchangability
 * would require the use of multiple inherritance using virtual base classes
 * and RTTI which is not supported in Mozilla's cross platform domain.
 *
 * The wrappers "own" their Mozilla counterparts through an nsCOMPtr, releasing
 * them when they get deleted. The wrappers are kept in a hashtable, owned by
 * the document to keep us from creating new wrappers for the same Mozilla
 * objects over and over. The hashtable takes care of deleting all the wrappers
 * when their owning document is destroyed.
 */

#ifndef MOZILLA_MITRE_DOM
#define MOZILLA_MITRE_DOM

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "pldhash.h"
#include "txAtom.h"
#include "TxObject.h"

#define kTxNsNodeIndexOffset 0x00000000;
#define kTxAttrIndexOffset 0x40000000;
#define kTxChildIndexOffset 0x80000000;

extern nsINameSpaceManager* gTxNameSpaceManager;

class nsIDOMAttr;
class nsIDOMDocument;
class nsIDOMElement;
class nsIDOMNamedNodeMap;
class nsIDOMNode;
class nsIDOMProcessingInstruction;

class Attr;
class Document;
class Element;
class NamedNodeMap;
class Node;
class ProcessingInstruction;

/**
 * This macro creates a nsCOMPtr to a specific interface for the
 * wrapper's Mozilla object. The nsCOMPtr will be named like the
 * supplied class with "ns" as a prefix.
 */
#define NSI_FROM_TX(_txClass)                                           \
nsCOMPtr<nsIDOM##_txClass> ns##_txClass(do_QueryInterface(mMozObject)); \
NS_ASSERTION(ns##_txClass, "This wrapper's Mozilla object went away!")

/**
 * Base wrapper class for a Mozilla object. Owns the Mozilla object through an
 * nsCOMPtr<nsISupports>.
 */
class MozillaObjectWrapper : public TxObject
{
public:
    /**
     * Construct a wrapper with the specified Mozilla object and document owner.
     *
     * @param aMozObject the Mozilla object you want to wrap
     * @param aOwnerDocument the document that owns this wrapper
     */
    MozillaObjectWrapper(nsISupports* aMozObject,
                         Document* aOwnerDocument)
        : mMozObject(aMozObject),
          mOwnerDocument(aOwnerDocument)
    {
        MOZ_COUNT_CTOR(MozillaObjectWrapper);
        NS_ASSERTION(aMozObject, "Wrapper needs Mozilla object!");
        NS_ASSERTION(aOwnerDocument, "Wrapper needs owner document!");
    };

    /**
     * Destructor
     */
    virtual ~MozillaObjectWrapper()
    {
        MOZ_COUNT_DTOR(MozillaObjectWrapper);
        NS_ASSERTION(inHashTableDeletion(),
                     "Never directly delete a DOM object, except a document!");

    };

    /**
     * Get the Mozilla object wrapped with this wrapper.
     *
     * @return the Mozilla object wrapped with this wrapper
     */
    nsISupports* getNSObj() const
    {
        return mMozObject;
    };

protected:
    // We want to maintain a pointer back to the owner document for memory
    // management.
    nsCOMPtr<nsISupports> mMozObject;
    Document* mOwnerDocument;

#ifdef DEBUG
private:
    // Defined in Document.cpp (debugging aid, needs access to
    // Document::mInHashTableDeletion).
    PRBool inHashTableDeletion();
#endif
};

/**
 * Wrapper class for nsIDOMNode. It provides the generic Node interface,
 * forwarding all functionality to its Mozilla counterpart.
 */
class Node : public MozillaObjectWrapper
{
public:
    // Node type constants
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

    Node(nsIDOMNode* aNode, Document* aOwner);
    virtual ~Node();

    // Read functions
    virtual nsresult getNodeName(nsAString& aName);
    virtual nsresult getNodeValue(nsAString& aValue);
    virtual unsigned short getNodeType() const;
    virtual Node* getParentNode();
    virtual Node* getFirstChild();
    virtual Node* getLastChild();
    virtual Node* getPreviousSibling();
    virtual Node* getNextSibling();
    virtual NamedNodeMap* getAttributes();
    virtual Document* getOwnerDocument();

    // Node manipulation functions
    virtual Node* appendChild(Node* aNewChild);

    virtual MBool hasChildNodes() const;

    // Introduced in DOM2
    virtual nsresult getNamespaceURI(nsAString& aNSURI);

    // From DOM3 26-Jan-2001 WD
    virtual nsresult getBaseURI(nsAString& aURI);

    // txXPathNode functions
    virtual MBool getLocalName(txAtom** aLocalName);
    virtual PRInt32 getNamespaceID();
    virtual PRInt32 lookupNamespaceID(txAtom* aPrefix);
    virtual Node* getXPathParent();
    virtual PRInt32 compareDocumentPosition(Node* aOther);

protected:
    PRInt32 mNamespaceID;
    
private:
    // Struct to hold document order information
    struct OrderInfo {
        ~OrderInfo();
        PRUint32* mOrder;
        PRInt32 mSize;
        Node* mRoot;
    };

    OrderInfo* mOrderInfo;

    // Helpfunctions for compareDocumentPosition
    OrderInfo* getOrderInfo();
};

/**
 * Wrapper class for nsIDOMNamedNodeMap.
 */
class NamedNodeMap : public MozillaObjectWrapper
{
public:
    NamedNodeMap(nsIDOMNamedNodeMap* aNamedNodeMap, Document* aOwner);
    ~NamedNodeMap();

    Node* getNamedItem(const nsAString& aName);
    Node* item(PRUint32 aIndex);
    PRUint32 getLength();
};

/**
 * Wrapper class for nsIDOMDocument.
 */
class Document : public Node
{
public:
    Document(nsIDOMDocument* aDocument);
    ~Document();

    Element* getDocumentElement();

    // Determine what kind of node this is, and create the appropriate
    // wrapper for it.
    Node* createWrapper(nsIDOMNode* node);

    // Factory functions for various node types.  These functions
    // are responsible for storing the wrapper classes they create in 
    // the document's wrapperHashTable.
    // Note the addition of the factory functions to "wrap"
    // nsIDOM* objects.
    Attr* createAttribute(nsIDOMAttr* aAttr);
    Element* createElement(nsIDOMElement* aElement);
    NamedNodeMap* createNamedNodeMap(nsIDOMNamedNodeMap* aMap);
    Node* createNode(nsIDOMNode* aNode);
    ProcessingInstruction* createProcessingInstruction(
                nsIDOMProcessingInstruction* aPi);

    Node* createComment(const nsAString& aData);
    Node* createDocumentFragment();
    ProcessingInstruction* createProcessingInstruction(const nsAString& aTarget,
                                                       const nsAString& aData);
    Node* createTextNode(const nsAString& aData);

    // Introduced in DOM Level 2
    Element* createElementNS(const nsAString& aNamespaceURI,
                             const nsAString& aTagName);

    Element* getElementById(const nsAString& aID);

    PRInt32 namespaceURIToID(const nsAString& aNamespaceURI);
    void namespaceIDToURI(PRInt32 aNamespaceID, nsAString& aNamespaceURI);

private:
    PLDHashTable mWrapperHashTable;
    PLDHashTable mAttributeNodes;

#ifdef DEBUG
    friend class MozillaObjectWrapper;

    PRBool mInHashTableDeletion;
#endif
};

/**
 * Wrapper class for nsIDOMElement.
 */
class Element : public Node
{
public:
    Element(nsIDOMElement* aElement, Document* aOwner);
    ~Element();

    void setAttributeNS(const nsAString& aNamespaceURI,
                        const nsAString& aName,
                        const nsAString& aValue);

    // txXPathNode functions
    MBool getLocalName(txAtom** aLocalName);
    MBool getAttr(txAtom* aLocalName, PRInt32 aNSID, nsAString& aValue);
    MBool hasAttr(txAtom* aLocalName, PRInt32 aNSID);
};

class txAttributeNodeKey
{
public:
    txAttributeNodeKey()
    {
    }
    txAttributeNodeKey(nsIContent* aParent,
                       nsIAtom* aLocalName, 
                       PRInt32 aNameSpaceId) : mParent(aParent),
                                               mLocalName(aLocalName),
                                               mNSId(aNameSpaceId)
    {
    }
    ~txAttributeNodeKey()
    {
    }
    PRBool Equals(const txAttributeNodeKey& aAttributeNodeKey) const
    {
        return (mParent == aAttributeNodeKey.mParent) &&
               (mLocalName == aAttributeNodeKey.mLocalName) &&
               (mNSId == aAttributeNodeKey.mNSId);
    }
    inline PRUint32 GetHash(void) const
    {
        return NS_PTR_TO_INT32(mParent.get()) ^
               (NS_PTR_TO_INT32(mLocalName.get()) << 12) ^
               (mNSId << 24);
    }

protected:
    nsCOMPtr<nsIContent> mParent;
    nsCOMPtr<nsIAtom> mLocalName;
    PRInt32 mNSId;
};

/**
 * Wrapper class for nsIDOMAttr.
 */
class Attr : public Node,
             public txAttributeNodeKey
{
public:
    Attr(nsIDOMAttr* aAttr, Document* aOwner);
    ~Attr();

    // txXPathNode functions override
    MBool getLocalName(txAtom** aLocalName);
    Node* getXPathParent();

    txAttributeNodeKey* GetKey() {
        return NS_STATIC_CAST(txAttributeNodeKey*, this);
    }
};

/**
 * Wrapper class for nsIDOMProcessingInstruction.
 */
class ProcessingInstruction : public Node
{
public:
    ProcessingInstruction(nsIDOMProcessingInstruction* aProcInstr, 
                          Document* aOwner);
    ~ProcessingInstruction();

    // txXPathNode functions
    MBool getLocalName(txAtom** aLocalName);
};

#endif
