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
 * Implementation of the wrapper class to convert the Mozilla nsIDOMDocument
 * interface into a TransforMIIX Document interface.
 */

#include "mozilladom.h"
#include "nsIDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMComment.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMElement.h"
#include "nsIDOMEntity.h"
#include "nsIDOMNotation.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"

// Need to determine if these are well-chosen.
#define WRAPPER_INITIAL_SIZE 256
#define ATTR_INITIAL_SIZE 128

struct txWrapperHashEntry : public PLDHashEntryHdr
{
    MozillaObjectWrapper* mWrapper;
};

PR_STATIC_CALLBACK(const void *)
txWrapperHashGetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    txWrapperHashEntry *e =
        NS_STATIC_CAST(txWrapperHashEntry *, entry);
    return e->mWrapper->getNSObj();
}

PR_STATIC_CALLBACK(void)
txWrapperHashClearEntry(PLDHashTable *table,
                        PLDHashEntryHdr *entry)
{
    txWrapperHashEntry *e = NS_STATIC_CAST(txWrapperHashEntry *, entry);
    // Don't delete the document wrapper, this is only called from its
    // destructor.
    if (e->mWrapper != table->data) {
        delete e->mWrapper;
    }
}

PR_STATIC_CALLBACK(PRBool)
txWrapperHashMatchEntry(PLDHashTable *table,
                        const PLDHashEntryHdr *entry,
                        const void *key)
{
    const txWrapperHashEntry *e =
        NS_STATIC_CAST(const txWrapperHashEntry *, entry);
    return e->mWrapper->getNSObj() == key;
}

struct txAttributeHashEntry : public PLDHashEntryHdr
{
    Attr* mAttribute;
};

PR_STATIC_CALLBACK(const void *)
txAttributeHashGetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    txAttributeHashEntry *e =
        NS_STATIC_CAST(txAttributeHashEntry *, entry);
    return e->mAttribute->GetKey();
}

PR_STATIC_CALLBACK(PLDHashNumber)
txAttributeHashHashKey(PLDHashTable *table, const void *key)
{
    const txAttributeNodeKey* attrKey =
        NS_STATIC_CAST(const txAttributeNodeKey *, key);
    return attrKey->GetHash();
}

PR_STATIC_CALLBACK(PRBool)
txAttributeHashMatchEntry(PLDHashTable *table,
                               const PLDHashEntryHdr *entry,
                               const void *key)
{
    const txAttributeHashEntry *e =
        NS_STATIC_CAST(const txAttributeHashEntry *, entry);
    const txAttributeNodeKey* key1 = e->mAttribute->GetKey();
    const txAttributeNodeKey* key2 =
        NS_STATIC_CAST(const txAttributeNodeKey *, key);

    return key1->Equals(*key2);
}

PR_STATIC_CALLBACK(void)
txAttributeHashClearEntry(PLDHashTable *table,
                          PLDHashEntryHdr *entry)
{
    txAttributeHashEntry *e =
        NS_STATIC_CAST(txAttributeHashEntry *, entry);
    delete e->mAttribute;
}

/**
 * Construct a wrapper with the specified Mozilla object. The caller is
 * responsible for deleting the wrapper object!
 *
 * @param aDocument the nsIDOMDocument you want to wrap
 */
Document::Document(nsIDOMDocument* aDocument) : Node(aDocument, this)
{
#ifdef DEBUG
    mInHashTableDeletion = PR_FALSE;
#endif

    static PLDHashTableOps wrapper_hash_table_ops =
    {
        PL_DHashAllocTable,
        PL_DHashFreeTable,
        txWrapperHashGetKey,
        PL_DHashVoidPtrKeyStub,
        txWrapperHashMatchEntry,
        PL_DHashMoveEntryStub,
        txWrapperHashClearEntry,
        PL_DHashFinalizeStub,
    };
    PRBool success = PL_DHashTableInit(&mWrapperHashTable,
                                       &wrapper_hash_table_ops,
                                       this,
                                       sizeof(txWrapperHashEntry),
                                       WRAPPER_INITIAL_SIZE);
    if (success) {
        txWrapperHashEntry* entry =
            NS_STATIC_CAST(txWrapperHashEntry *,
                           PL_DHashTableOperate(&mWrapperHashTable,
                                                aDocument,
                                                PL_DHASH_ADD));

        NS_ASSERTION(entry, "Out-of-memory creating an entry.");
        if (entry && !entry->mWrapper) {
            entry->mWrapper = this;
        }
    }
    else {
        mWrapperHashTable.ops = nsnull;
    }

    static PLDHashTableOps attribute_hash_table_ops =
    {
        PL_DHashAllocTable,
        PL_DHashFreeTable,
        txAttributeHashGetKey,
        txAttributeHashHashKey,
        txAttributeHashMatchEntry,
        PL_DHashMoveEntryStub,
        txAttributeHashClearEntry,
        PL_DHashFinalizeStub,
    };
    success = PL_DHashTableInit(&mAttributeNodes,
                                &attribute_hash_table_ops,
                                nsnull,
                                sizeof(txAttributeHashEntry),
                                ATTR_INITIAL_SIZE);
    if (!success) {
        mAttributeNodes.ops = nsnull;
    }
}

/**
 * Destructor
 */
Document::~Document()
{
#ifdef DEBUG
    mInHashTableDeletion = PR_TRUE;
#endif
    if (mAttributeNodes.ops) {
        PL_DHashTableFinish(&mAttributeNodes);
    }
    if (mWrapperHashTable.ops) {
        PL_DHashTableFinish(&mWrapperHashTable);
    }
}

/**
 * This macro can be used to declare a createWrapper implementation
 * for the supplied wrapper class.
 */
#define IMPL_CREATE_WRAPPER(_txClass)                           \
IMPL_CREATE_WRAPPER2(_txClass, create##_txClass)

/**
 * This macro can be used to declare a createWrapper implementation
 * for the supplied wrapper class. The function parameter defines
 * the function name for the implementation function.
 */
#define IMPL_CREATE_WRAPPER2(_txClass, _function)               \
_txClass* Document::_function(nsIDOM##_txClass* aNsObject)      \
{                                                               \
    NS_ASSERTION(aNsObject,                                     \
                 "Need a Mozilla object to create a wrapper."); \
    if (!mWrapperHashTable.ops) {                               \
        return new _txClass(aNsObject, this);                   \
    }                                                           \
                                                                \
    txWrapperHashEntry* entry =                                 \
        NS_STATIC_CAST(txWrapperHashEntry *,                    \
                       PL_DHashTableOperate(&mWrapperHashTable, \
                                            aNsObject,          \
                                            PL_DHASH_ADD));     \
                                                                \
    NS_ASSERTION(entry, "Out-of-memory creating an entry.");    \
    if (!entry) {                                               \
        return nsnull;                                          \
    }                                                           \
    if (!entry->mWrapper) {                                     \
        entry->mWrapper = new _txClass(aNsObject, this);        \
        NS_ASSERTION(entry->mWrapper,                           \
                     "Out-of-memory creating a wrapper.");      \
        if (!entry->mWrapper) {                                 \
            PL_DHashTableRawRemove(&mWrapperHashTable, entry);  \
            return nsnull;                                      \
        }                                                       \
    }                                                           \
    return NS_STATIC_CAST(_txClass*, entry->mWrapper);          \
}

/**
 * Call nsIDOMDocument::GetDocumentElement to get the document's
 * DocumentElement.
 *
 * @return the document element
 */
Element* Document::getDocumentElement()
{
    NSI_FROM_TX(Document);
    nsCOMPtr<nsIDOMElement> element;
    nsDocument->GetDocumentElement(getter_AddRefs(element));
    if (!element) {
        return nsnull;
    }
    return createElement(element);
}

/**
 * Call nsIDOMDocument::GetDoctype to get the document's DocumentType.
 *
 * @return the DocumentType
 */
DocumentType* Document::getDoctype()
{
    NSI_FROM_TX(Document);
    nsCOMPtr<nsIDOMDocumentType> docType;
    nsDocument->GetDoctype(getter_AddRefs(docType));
    if (!docType) {
        return nsnull;
    }
    return createDocumentType(docType);
}

/**
 * Call nsIDOMDocument::CreateDocumentFragment to create a DocumentFragment.
 *
 * @return the DocumentFragment
 */
DocumentFragment* Document::createDocumentFragment()
{
    NSI_FROM_TX(Document);
    nsCOMPtr<nsIDOMDocumentFragment> fragment;
    nsDocument->CreateDocumentFragment(getter_AddRefs(fragment));
    if (!fragment) {
        return nsnull;
    }
    return createNode(fragment);
}

/**
 * Call nsIDOMDocument::GetElementById to get Element with ID.
 *
 * @param aID the name of ID referencing the element
 *
 * @return the Element
 */
Element* Document::getElementById(const String aID)
{
    NSI_FROM_TX(Document);
    nsCOMPtr<nsIDOMElement> element;
    nsDocument->GetElementById(aID, getter_AddRefs(element));
    if (!element) {
        return nsnull;
    }
    return createElement(element);
}

/**
 * Create a wrapper for a nsIDOMElement, reuses an existing wrapper if possible.
 *
 * @param aElement the nsIDOMElement you want to wrap
 *
 * @return the Element
 */
IMPL_CREATE_WRAPPER(Element)

/**
 * Call nsIDOMDocument::CreateElementNS to create an Element.
 *
 * @param aNamespaceURI the URI of the namespace for the element
 * @param aTagName the name of the element you want to create
 *
 * @return the Element
 */
Element* Document::createElementNS(const String& aNamespaceURI,
                                   const String& aTagName)
{
    NSI_FROM_TX(Document);
    nsCOMPtr<nsIDOMElement> element;
    nsDocument->CreateElementNS(aNamespaceURI, aTagName,
                                getter_AddRefs(element));
    if (!element) {
        return nsnull;
    }
    return createElement(element);
}

/**
 * Create a wrapper for a nsIDOMAttr, reuses an existing wrapper if possible.
 *
 * @param aAttr the nsIDOMAttr you want to wrap
 *
 * @return the Attribute
 */
Attr* Document::createAttribute(nsIDOMAttr* aAttr)
{
    NS_ASSERTION(aAttr,
                 "Need a Mozilla object to create a wrapper.");
    if (!aAttr) {
        return nsnull;
    }

    nsCOMPtr<nsIDOMElement> owner;
    aAttr->GetOwnerElement(getter_AddRefs(owner));
    nsCOMPtr<nsIContent> parent = do_QueryInterface(owner);

    nsAutoString nameString;
    aAttr->GetLocalName(nameString);
    nsCOMPtr<nsIAtom> localName = do_GetAtom(nameString);

    nsAutoString ns;
    aAttr->GetNamespaceURI(ns);
    PRInt32 namespaceID = kNameSpaceID_None;
    if (!ns.IsEmpty()) {
        NS_ASSERTION(gTxNameSpaceManager, "No namespace manager");
        if (gTxNameSpaceManager) {
            gTxNameSpaceManager->GetNameSpaceID(ns, namespaceID);
        }
    }

    if (!mAttributeNodes.ops) {
        return nsnull;
    }

    txAttributeNodeKey hashKey(parent, localName, namespaceID);

    txAttributeHashEntry* entry =
        NS_STATIC_CAST(txAttributeHashEntry *,
                       PL_DHashTableOperate(&mAttributeNodes,
                                            &hashKey,
                                            PL_DHASH_ADD));

    NS_ASSERTION(entry, "Out-of-memory creating an entry.");
    if (!entry) {
        return nsnull;
    }
    if (!entry->mAttribute) {
        entry->mAttribute = new Attr(aAttr, this);
        NS_ASSERTION(entry->mAttribute,
                     "Out-of-memory creating an attribute wrapper.");
        if (!entry->mAttribute) {
            PL_DHashTableRawRemove(&mAttributeNodes, entry);
            return nsnull;
        }
    }

    return entry->mAttribute;
}

/**
 * Call nsIDOMDocument::CreateTextNode to create a Text node.
 *
 * @param aData the data of the text node you want to create
 *
 * @return the Text node
 */
Text* Document::createTextNode(const String& aData)
{
    NSI_FROM_TX(Document);
    nsCOMPtr<nsIDOMText> text;
    nsDocument->CreateTextNode(aData, getter_AddRefs(text));
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(text);
    if (!node) {
        return nsnull;
    }
    return createNode(node);
}

/**
 * Call nsIDOMDocument::CreateComment to create a Comment node.
 *
 * @param aData the data of the comment node you want to create
 *
 * @return the Comment node
 */
Comment* Document::createComment(const String& aData)
{
    NSI_FROM_TX(Document);
    nsCOMPtr<nsIDOMComment> comment;
    nsDocument->CreateComment(aData, getter_AddRefs(comment));
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(comment);
    if (!node) {
        return nsnull;
    }
    return createNode(node);
}

/**
 * Call nsIDOMDocument::CreateProcessingInstruction to create a
 * ProcessingInstruction.
 *
 * @param aTarget the target of the ProcessingInstruction you want to create
 * @param aData the data of the ProcessingInstruction you want to create
 *
 * @return the ProcessingInstruction node
 */
ProcessingInstruction* Document::createProcessingInstruction(
            const String& aTarget, const String& aData)
{
    NSI_FROM_TX(Document);
    nsCOMPtr<nsIDOMProcessingInstruction> pi;
    nsDocument->CreateProcessingInstruction(aTarget, aData,
                                            getter_AddRefs(pi));
    if (!pi) {
        return nsnull;
    }
    return createProcessingInstruction(pi);
}

/**
 * Create a wrapper for a nsIDOMProcessingInstruction, reuses an existing
 * wrapper if possible.
 *
 * @param aPi the nsIDOMProcessingInstruction you want to wrap
 *
 * @return the ProcessingInstruction node
 */
IMPL_CREATE_WRAPPER(ProcessingInstruction)

/**
 * Create a wrapper for a nsIDOMEntity, reuses an existing wrapper if possible.
 *
 * @param aEntity the nsIDOMEntity you want to wrap
 *
 * @return the Entity
 */
IMPL_CREATE_WRAPPER(Entity)

/**
 * Create a wrapper for a nsIDOMNode, reuses an existing wrapper if possible.
 *
 * @param aNode the nsIDOMNode you want to wrap
 *
 * @return the Node
 */
IMPL_CREATE_WRAPPER(Node)

/**
 * Create a wrapper for a nsIDOMNotation, reuses an existing wrapper if
 * possible.
 *
 * @param aNotation the nsIDOMNotation you want to wrap
 *
 * @return the Notation
 */
IMPL_CREATE_WRAPPER(Notation)

/**
 * Create a wrapper for a nsIDOMDocumentType, reuses an existing wrapper if
 * possible.
 *
 * @param aDoctype the nsIDOMDocumentType you want to wrap
 *
 * @return the DocumentType
 */
IMPL_CREATE_WRAPPER(DocumentType)

/**
 * Create a wrapper for a nsIDOMNodeList, reuses an existing wrapper if
 * possible.
 *
 * @param aList the nsIDOMNodeList you want to wrap
 *
 * @return the NodeList
 */
IMPL_CREATE_WRAPPER(NodeList)

/**
 * Create a wrapper for a nsIDOMNamedNodeMap, reuses an existing wrapper if
 * possible.
 *
 * @param aMap the nsIDOMNamedNodeMap you want to wrap
 *
 * @return the NamedNodeMap
 */
IMPL_CREATE_WRAPPER(NamedNodeMap)

/**
 * Create a wrapper for a nsIDOMNode, reuses an existing wrapper if possible.
 * This function creates the right kind of wrapper depending on the type of
 * the node.
 *
 * @param aNode the nsIDOMNode you want to wrap
 *
 * @return the Node
 */
Node* Document::createWrapper(nsIDOMNode* aNode)
{
    NS_ASSERTION(aNode,
                 "Need a Mozilla object to create a wrapper.");

    PRUint16 nodeType = 0;
    aNode->GetNodeType(&nodeType);

    // Look up wrapper in the hash, except for attribute nodes since
    // they're in a separate attribute hash. Let them be handled in
    // createAttribute by falling through to the switch below.
    if (nodeType != nsIDOMNode::ATTRIBUTE_NODE &&
        mWrapperHashTable.ops) {
        txWrapperHashEntry* entry =
            NS_STATIC_CAST(txWrapperHashEntry *,
                           PL_DHashTableOperate(&mWrapperHashTable,
                                                aNode,
                                                PL_DHASH_LOOKUP));
        if (entry->mWrapper) {
            return NS_STATIC_CAST(Node*, entry->mWrapper);
        }
    }

    switch (nodeType)
    {
        case nsIDOMNode::ELEMENT_NODE:
        {
            nsIDOMElement* element;
            CallQueryInterface(aNode, &element);
            Element* txElement = createElement(element);
            NS_RELEASE(element);
            return txElement;
            break;
        }
        case nsIDOMNode::ATTRIBUTE_NODE:
        {
            nsIDOMAttr* attr;
            CallQueryInterface(aNode, &attr);
            Attr* txAttr = createAttribute(attr);
            NS_RELEASE(attr);
            return txAttr;
            break;
        }
        case nsIDOMNode::CDATA_SECTION_NODE:
        case nsIDOMNode::COMMENT_NODE:
        case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
        case nsIDOMNode::ENTITY_REFERENCE_NODE:
        case nsIDOMNode::TEXT_NODE:
        {
            return createNode(aNode);
            break;
        }
        case nsIDOMNode::ENTITY_NODE:
        {
            nsIDOMEntity* entity;
            CallQueryInterface(aNode, &entity);
            Entity* txEntity = createEntity(entity);
            NS_RELEASE(entity);
            return txEntity;
            break;
        }
        case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
        {
            nsIDOMProcessingInstruction* pi;
            CallQueryInterface(aNode, &pi);
            ProcessingInstruction* txPi = createProcessingInstruction(pi);
            NS_RELEASE(pi);
            return txPi;
            break;
        }
        case nsIDOMNode::DOCUMENT_NODE:
        {
            if (aNode == mMozObject) {
                return this;
            }
            NS_ASSERTION(0, "We don't support creating new documents.");
            return nsnull;
            break;
        }
        case nsIDOMNode::DOCUMENT_TYPE_NODE:
        {
            nsIDOMDocumentType* docType;
            CallQueryInterface(aNode, &docType);
            DocumentType* txDocType = createDocumentType(docType);
            NS_RELEASE(docType);
            return txDocType;
            break;
        }
        case nsIDOMNode::NOTATION_NODE:
        {
            nsIDOMNotation* notation;
            CallQueryInterface(aNode, &notation);
            Notation* txNotation = createNotation(notation);
            NS_RELEASE(notation);
            return txNotation;
        }
        default:
        {
            NS_WARNING("Don't know nodetype. This could be a failure.");
            return createNode(aNode);
        }
    }
}

PRInt32 Document::namespaceURIToID(const String& aNamespaceURI)
{
    PRInt32 namesspaceID = kNameSpaceID_Unknown;
    if (gTxNameSpaceManager) {
        gTxNameSpaceManager->RegisterNameSpace(aNamespaceURI,
                                             namesspaceID);
    }
    return namesspaceID;
}

void Document::namespaceIDToURI(PRInt32 aNamespaceID, String& aNamespaceURI)
{
    if (gTxNameSpaceManager) {
        gTxNameSpaceManager->GetNameSpaceURI(aNamespaceID,
                                           aNamespaceURI);
    }
}

#ifdef DEBUG
PRBool MozillaObjectWrapper::inHashTableDeletion()
{
    return mOwnerDocument->mInHashTableDeletion;
}
#endif
