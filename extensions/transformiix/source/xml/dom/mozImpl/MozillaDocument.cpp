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

/* Implementation of the wrapper class to convert the Mozilla nsIDOMDocument
   interface into a TransforMIIX Document interface.
*/

#include "mozilladom.h"
#include "nsLayoutCID.h"
#include "nsIURL.h"

static NS_DEFINE_CID(kIDOMDOMImplementationCID, NS_DOM_IMPLEMENTATION_CID);

PR_STATIC_CALLBACK(PRBool)
DeleteWrapper(nsHashKey *aKey, void *aData, void* closure)
{
    delete (MozillaObjectWrapper*)aData;
    return PR_TRUE;
}

/**
 * Construct a new Mozilla document and wrap it. The caller is responsible for
 * deleting the wrapper object!
 */
Document::Document() : Node(0, 0)
{
    nsresult res;
    nsCOMPtr<nsIDOMDocument> document;

    nsCOMPtr<nsIDOMDOMImplementation> implementation =
                do_CreateInstance(kIDOMDOMImplementationCID, &res);
    //if (NS_FAILED(res)) return NULL;

    // Create an empty document from it
    nsAutoString emptyStr;
    res = implementation->CreateDocument(emptyStr, emptyStr, nsnull,
                getter_AddRefs(document));
    //if (NS_FAILED(res)) return NULL;

    ownerDocument = this;
    wrapperHashTable = new nsObjectHashtable(nsnull, nsnull,
                                             DeleteWrapper, nsnull);
    bInHashTableDeletion = PR_FALSE;
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(document));
    NS_ASSERTION(doc,"document doesn't implement nsIDocument");
    if (doc) {
        doc->GetNameSpaceManager(*getter_AddRefs(nsNSManager));
        NS_ASSERTION(nsNSManager, "Unable to get nsINamespaceManager");
    }
    addWrapper(this);
}

/**
 * Construct a wrapper with the specified Mozilla object. The caller is
 * responsible for deleting the wrapper object!
 *
 * @param aDocument the nsIDOMDocument you want to wrap
 */
Document::Document(nsIDOMDocument* aDocument) : Node(aDocument, 0)
{
    ownerDocument = this;
    wrapperHashTable = new nsObjectHashtable(nsnull, nsnull,
                                             DeleteWrapper, nsnull);
    bInHashTableDeletion = PR_FALSE;
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDocument));
    NS_ASSERTION(doc,"document doesn't implement nsIDocument");
    if (doc) {
        doc->GetNameSpaceManager(*getter_AddRefs(nsNSManager));
        NS_ASSERTION(nsNSManager, "Unable to get nsINamespaceManager");
    }
    addWrapper(this);
}

/**
 * Destructor
 */
Document::~Document()
{
    removeWrapper(this);
    bInHashTableDeletion = PR_TRUE;
    delete wrapperHashTable;
}

/**
 * Flags wether the document is deleting the wrapper hash table and the objects
 * that it contains.
 *
 * @return flags wether the document is deleting the wrapper hash table
 */
PRBool Document::inHashTableDeletion()
{
    return bInHashTableDeletion;
}

/**
 * Call nsIDOMDocument::GetDocumentElement to get the document's
 * DocumentElement.
 *
 * @return the document element
 */
Element* Document::getDocumentElement()
{
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMElement> theElement;

    if (NS_SUCCEEDED(nsDocument->GetDocumentElement(
                getter_AddRefs(theElement))))
        return (Element*)createWrapper(theElement);
    else
        return NULL;
}

/**
 * Call nsIDOMDocument::GetDoctype to get the document's DocumentType.
 *
 * @return the DocumentType
 */
DocumentType* Document::getDoctype()
{
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMDocumentType> theDocType;

    if (NS_SUCCEEDED(nsDocument->GetDoctype(getter_AddRefs(theDocType))))
        return (DocumentType*)createWrapper(theDocType);
    else
        return NULL;
}

/**
 * Call nsIDOMDocument::GetImplementation to get the document's
 * DOMImplementation.
 *
 * @return the DOMImplementation
 */
DOMImplementation* Document::getImplementation()
{
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMDOMImplementation> theImpl;

    if (NS_SUCCEEDED(nsDocument->GetImplementation(getter_AddRefs(theImpl))))
        return createDOMImplementation(theImpl);
    else
        return NULL;
}

/**
 * Call nsIDOMDocument::CreateDocumentFragment to create a DocumentFragment.
 *
 * @return the DocumentFragment
 */
DocumentFragment* Document::createDocumentFragment()
{
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMDocumentFragment> fragment;

    if (NS_SUCCEEDED(nsDocument->CreateDocumentFragment(
                getter_AddRefs(fragment))))
        return createDocumentFragment(fragment);
    else
        return NULL;
}

/**
 * Create a wrapper for a nsIDOMDocumentFragment, reuses an existing wrapper if
 * possible.
 *
 * @param aFragment the nsIDOMDocumentFragment you want to wrap
 *
 * @return the DocumentFragment
 */
IMPL_CREATE_WRAPPER(DocumentFragment)

/**
 * Call nsIDOMDocument::CreateElement to create an Element.
 *
 * @param aTagName the name of the element you want to create
 *
 * @return the Element
 */
Element* Document::createElement(const String& aTagName)
{
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMElement> element;

    if (NS_SUCCEEDED(nsDocument->CreateElement(aTagName.getConstNSString(),
                getter_AddRefs(element))))
        return createElement(element);
    else
        return NULL;
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
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMElement> element;

    if (NS_SUCCEEDED(nsDocument->GetElementById(aID.getConstNSString(), getter_AddRefs(element))))
        return createElement(element);
    else
        return NULL;
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
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMElement> element;

    if (NS_SUCCEEDED(nsDocument->CreateElementNS(
                aNamespaceURI.getConstNSString(), aTagName.getConstNSString(),
                getter_AddRefs(element))))
        return createElement(element);
    else
        return NULL;
}

/**
 * Call nsIDOMDocument::CreateAttribute to create an Attribute.
 *
 * @param aName the name of the attribute you want to create
 *
 * @return the Attribute
 */
Attr* Document::createAttribute(const String& aName)
{
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMAttr> attr;

    if (NS_SUCCEEDED(nsDocument->CreateAttribute(aName.getConstNSString(),
                getter_AddRefs(attr))))
        return createAttribute(attr);
    else
        return NULL;
}

/**
 * Call nsIDOMDocument::CreateAttributeNS to create an Attribute.
 *
 * @param aNamespaceURI the URI of the namespace for the element
 * @param aName the name of the attribute you want to create
 *
 * @return the Attribute
 */
Attr* Document::createAttributeNS(const String& aNamespaceURI,
            const String& aName)
{
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMAttr> attr;

    if (NS_SUCCEEDED(nsDocument->CreateAttributeNS(
                aNamespaceURI.getConstNSString(), aName.getConstNSString(),
                getter_AddRefs(attr))))
        return createAttribute(attr);
    else
        return NULL;
}

/**
 * Create a wrapper for a nsIDOMAttr, reuses an existing wrapper if possible.
 *
 * @param aAttr the nsIDOMAttr you want to wrap
 *
 * @return the Attribute
 */
IMPL_CREATE_WRAPPER2(Attr, createAttribute)

/**
 * Call nsIDOMDocument::CreateTextNode to create a Text node.
 *
 * @param aData the data of the text node you want to create
 *
 * @return the Text node
 */
Text* Document::createTextNode(const String& aData)
{
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMText> text;

    if (NS_SUCCEEDED(nsDocument->CreateTextNode(aData.getConstNSString(),
                getter_AddRefs(text))))
        return createTextNode(text);
    else
        return NULL;
}

/**
 * Create a wrapper for a nsIDOMText, reuses an existing wrapper if possible.
 *
 * @param aText the nsIDOMText you want to wrap
 *
 * @return the Text node
 */
IMPL_CREATE_WRAPPER2(Text, createTextNode)

/**
 * Call nsIDOMDocument::CreateComment to create a Comment node.
 *
 * @param aData the data of the comment node you want to create
 *
 * @return the Comment node
 */
Comment* Document::createComment(const String& aData)
{
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMComment> comment;

    if (NS_SUCCEEDED(nsDocument->CreateComment(aData.getConstNSString(),
                getter_AddRefs(comment))))
        return createComment(comment);
    else
        return NULL;
}

/**
 * Create a wrapper for a nsIDOMComment, reuses an existing wrapper if possible.
 *
 * @param aComment the nsIDOMComment you want to wrap
 *
 * @return the Comment node
 */
IMPL_CREATE_WRAPPER(Comment)

/**
 * Call nsIDOMDocument::CreateCDATASection to create a CDataSection.
 *
 * @param aData the data of the CDataSection you want to create
 *
 * @return the CDataSection node
 */
CDATASection* Document::createCDATASection(const String& aData)
{
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMCDATASection> cdata;

    if (NS_SUCCEEDED(nsDocument->CreateCDATASection(aData.getConstNSString(),
                getter_AddRefs(cdata))))
        return createCDATASection(cdata);
    else
        return NULL;
}

/**
 * Create a wrapper for a nsIDOMCDATASection, reuses an existing wrapper if
 * possible.
 *
 * @param aCdata the nsIDOMCDATASection you want to wrap
 *
 * @return the CDATASection node
 */
IMPL_CREATE_WRAPPER(CDATASection)

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
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMProcessingInstruction> pi;

    if (NS_SUCCEEDED(nsDocument->CreateProcessingInstruction(
                aTarget.getConstNSString(), aData.getConstNSString(),
                getter_AddRefs(pi))))
        return createProcessingInstruction(pi);
    else
        return NULL;
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
 * Call nsIDOMDocument::CreateEntityReference to create a EntityReference.
 *
 * @param aName the name of the EntityReference you want to create
 *
 * @return the EntityReference
 */
EntityReference* Document::createEntityReference(const String& aName)
{
    NSI_FROM_TX_NULL_CHECK(Document)
    nsCOMPtr<nsIDOMEntityReference> entityRef;

    if (NS_SUCCEEDED(nsDocument->CreateEntityReference(aName.getConstNSString(),
                getter_AddRefs(entityRef))))
        return createEntityReference(entityRef);
    else
        return NULL;
}

/**
 * Create a wrapper for a nsIDOMEntityReference, reuses an existing
 * wrapper if possible.
 *
 * @param aEntityRef the nsIDOMEntityReference you want to wrap
 *
 * @return the EntityReference
 */
IMPL_CREATE_WRAPPER(EntityReference)

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
 * Create a wrapper for a nsIDOMDOMImplementation, reuses an existing wrapper if
 * possible.
 *
 * @param aImpl the nsIDOMDOMImplementation you want to wrap
 *
 * @return the DOMImplementation
 */
IMPL_CREATE_WRAPPER(DOMImplementation)

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
    unsigned short nodeType = 0;

    if (!aNode)
      return NULL;

    aNode->GetNodeType(&nodeType);

    switch (nodeType)
    {
        case nsIDOMNode::ELEMENT_NODE:
            return createElement((nsIDOMElement*)aNode);
            break;

        case nsIDOMNode::ATTRIBUTE_NODE:
            return createAttribute((nsIDOMAttr*)aNode);
            break;

        case nsIDOMNode::TEXT_NODE:
            return createTextNode((nsIDOMText*)aNode);
            break;

        case nsIDOMNode::CDATA_SECTION_NODE:
            return createCDATASection((nsIDOMCDATASection*)aNode);
           break;

        case nsIDOMNode::ENTITY_REFERENCE_NODE:
            return createEntityReference((nsIDOMEntityReference*)aNode);
            break;

        case nsIDOMNode::ENTITY_NODE:
            return createEntity((nsIDOMEntity*)aNode);
            break;

        case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
            return createProcessingInstruction(
                        (nsIDOMProcessingInstruction*)aNode);
            break;

        case nsIDOMNode::COMMENT_NODE:
            return createComment((nsIDOMComment*)aNode);
            break;

        case nsIDOMNode::DOCUMENT_NODE:
            if (aNode == getNSObj())
                return this;
            else {
                // XXX (pvdb) We need a createDocument here!
                return createNode(aNode);
            }
            break;

        case nsIDOMNode::DOCUMENT_TYPE_NODE:
            return createDocumentType((nsIDOMDocumentType*)aNode);
            break;

        case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
            return createDocumentFragment((nsIDOMDocumentFragment*)aNode);
            break;

        case nsIDOMNode::NOTATION_NODE:
            return createNotation((nsIDOMNotation*)aNode);
            break;

        default:
            return createNode(aNode);
    }
}

/**
 * Add a wrapper to the document's hash table using the specified hash value.
 *
 * @param aObj the TxObject you want to add
 * @param aHashValue the key for the object in the hash table
 */
void Document::addWrapper(MozillaObjectWrapper* aObject)
{
    nsISupportsKey key(aObject->getNSObj());
    wrapperHashTable->Put(&key, aObject);
}

/**
 * Remove a wrapper from the document's hash table and return it to the caller.
 *
 * @param aHashValue the key for the object you want to remove
 *
 * @return the wrapper as a TxObject
 */
TxObject* Document::removeWrapper(nsISupports* aMozillaObject)
{
    nsISupportsKey key(aMozillaObject);
    return (TxObject*)wrapperHashTable->Remove(&key);
}

/**
 * Remove a wrapper from the document's hash table and return it to the caller.
 *
 * @param aHashValue the key for the object you want to remove
 *
 * @return the wrapper as a TxObject
 */
TxObject* Document::removeWrapper(MozillaObjectWrapper* aObject)
{
    nsISupportsKey key(aObject->getNSObj());
    return (TxObject*)wrapperHashTable->Remove(&key);
}
