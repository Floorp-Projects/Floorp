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

/* Implementation of the wrapper class to convert the Mozilla nsIDOMDocument
   interface into a TransforMIIX Document interface.
*/

#include "mozilladom.h"
#include "nsLayoutCID.h"

static NS_DEFINE_CID(kIDOMDOMImplementationCID, NS_DOM_IMPLEMENTATION_CID);

/**
 * Construct a new Mozilla document and wrap it. The caller is responsible for
 * deleting the wrapper object!
 */
Document::Document() : Node(NULL, NULL)
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
    nsDocument = document;
    bInHashTableDeletion = PR_FALSE;
    addWrapper(this, getKey());
}

/**
 * Construct a wrapper with the specified Mozilla object. The caller is
 * responsible for deleting the wrapper object!
 *
 * @param aDocument the nsIDOMDocument you want to wrap
 */
Document::Document(nsIDOMDocument* aDocument) : Node(aDocument, this)
{
    nsDocument = aDocument;
    bInHashTableDeletion = PR_FALSE;
    addWrapper(this, getKey());
}

/**
 * Destructor
 */
Document::~Document()
{
    removeWrapper(getKey());
    bInHashTableDeletion = PR_TRUE;
}

/**
 * Wrap a different Mozilla object with this wrapper.
 *
 * @param aDocument the nsIDOMDocument you want to wrap
 */
void Document::setNSObj(nsIDOMDocument* aDocument)
{
    Node::setNSObj(aDocument);
    nsDocument = aDocument;
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
    nsCOMPtr<nsIDOMElement> theElement;
    Element* elemWrapper = NULL;

    if (nsDocument == NULL)
      return NULL;

    if (NS_SUCCEEDED(nsDocument->GetDocumentElement(
                getter_AddRefs(theElement))))
        return (Element*)createWrapper(theElement);
    else
        return NULL;
}

/**
 * Call nsIDOMDocument::GetDocumentElement to get the document's DocumentType.
 *
 * @return the DocumentType
 */
DocumentType* Document::getDoctype()
{
    nsCOMPtr<nsIDOMDocumentType> theDocType;

    if (nsDocument == NULL)
        return NULL;

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
    nsCOMPtr<nsIDOMDOMImplementation> theImpl;

    if (nsDocument == NULL)
        return NULL;

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
    nsCOMPtr<nsIDOMDocumentFragment> fragment;

    if (nsDocument == NULL)
        return NULL;

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
DocumentFragment* Document::createDocumentFragment(
            nsIDOMDocumentFragment* aFragment)
{
    DocumentFragment* docFragWrapper = NULL;

    if (aFragment)
    {
        docFragWrapper =
            (DocumentFragment*)wrapperHashTable.retrieve(aFragment);

        if (!docFragWrapper)
            docFragWrapper = new DocumentFragment(aFragment, this);
    }

    return docFragWrapper;
}

/**
 * Call nsIDOMDocument::CreateElement to create an Element.
 *
 * @param aTagName the name of the element you want to create
 *
 * @return the Element
 */
Element* Document::createElement(const String& aTagName)
{
    nsCOMPtr<nsIDOMElement> element;

    if (NS_SUCCEEDED(nsDocument->CreateElement(aTagName.getConstNSString(),
                getter_AddRefs(element))))
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
Element* Document::createElement(nsIDOMElement* aElement)
{
    Element* elemWrapper = NULL;

    if (aElement)
    {
        elemWrapper = (Element*)wrapperHashTable.retrieve(aElement);

        if (!elemWrapper)
            elemWrapper = new Element(aElement, this);
    }

    return elemWrapper;
}

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
    nsCOMPtr<nsIDOMAttr> attr;

    if (nsDocument == NULL)
        return NULL;

    if (nsDocument->CreateAttribute(aName.getConstNSString(),
                getter_AddRefs(attr)) == NS_OK)
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
Attr* Document::createAttribute(nsIDOMAttr* aAttr)
{
    Attr* attrWrapper = NULL;

    if (aAttr)
    {
        attrWrapper = (Attr*)wrapperHashTable.retrieve(aAttr);

        if (!attrWrapper)
            attrWrapper = new Attr(aAttr, this);
    }

    return attrWrapper;
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
    nsCOMPtr<nsIDOMText> text;

    if (nsDocument == NULL)
        return NULL;

    if (nsDocument->CreateTextNode(aData.getConstNSString(),
                getter_AddRefs(text)) == NS_OK)
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
Text* Document::createTextNode(nsIDOMText* aText)
{
    Text* textWrapper = NULL;

    if (aText)
    {
        textWrapper = (Text*)wrapperHashTable.retrieve(aText);

        if (!textWrapper)
            textWrapper = new Text(aText, this);
    }

    return textWrapper;
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
    nsCOMPtr<nsIDOMComment> comment;

    if (nsDocument == NULL)
        return NULL;

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
Comment* Document::createComment(nsIDOMComment* aComment)
{
    Comment* commentWrapper = NULL;

    if (aComment)
    {
        commentWrapper = (Comment*)wrapperHashTable.retrieve(aComment);

        if (!commentWrapper)
            commentWrapper = new Comment(aComment, this);
    }

    return commentWrapper;
}

/**
 * Call nsIDOMDocument::CreateCDATASection to create a CDataSection.
 *
 * @param aData the data of the CDataSection you want to create
 *
 * @return the CDataSection node
 */
CDATASection* Document::createCDATASection(const String& aData)
{
    nsCOMPtr<nsIDOMCDATASection> cdata;

    if (nsDocument == NULL)
        return NULL;

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
CDATASection* Document::createCDATASection(nsIDOMCDATASection* aCdata)
{
    CDATASection* cdataWrapper = NULL;

    if (cdataWrapper)
    {
        cdataWrapper = (CDATASection*)wrapperHashTable.retrieve(aCdata);

        if (!cdataWrapper)
            cdataWrapper = new CDATASection(aCdata, this);
    }

    return cdataWrapper;
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
    nsCOMPtr<nsIDOMProcessingInstruction> pi;

    if (nsDocument == NULL)
        return NULL;

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
ProcessingInstruction* Document::createProcessingInstruction(
            nsIDOMProcessingInstruction* aPi)
{
    ProcessingInstruction* piWrapper;

    if (aPi)
    {
        piWrapper = (ProcessingInstruction*)wrapperHashTable.retrieve(aPi);

        if (!piWrapper)
            piWrapper = new ProcessingInstruction(aPi, this);
    }

    return piWrapper;
}

/**
 * Call nsIDOMDocument::CreateEntityReference to create a EntityReference.
 *
 * @param aName the name of the EntityReference you want to create
 *
 * @return the EntityReference
 */
EntityReference* Document::createEntityReference(const String& aName)
{
    nsCOMPtr<nsIDOMEntityReference> entityRef;

    if (nsDocument == NULL)
        return NULL;

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
EntityReference* Document::createEntityReference(
            nsIDOMEntityReference* aEntityRef)
{
    EntityReference* entityWrapper = NULL;

    if (aEntityRef)
    {
        entityWrapper = (EntityReference*) wrapperHashTable.retrieve(
                    aEntityRef);

        if (!entityWrapper)
            entityWrapper = new EntityReference(aEntityRef, this);
    }

    return entityWrapper;
}

/**
 * Create a wrapper for a nsIDOMEntity, reuses an existing wrapper if possible.
 *
 * @param aEntity the nsIDOMEntity you want to wrap
 *
 * @return the Entity
 */
Entity* Document::createEntity(nsIDOMEntity* aEntity)
{
    Entity* entityWrapper = NULL;

    if (aEntity)
    {
        entityWrapper = (Entity*)wrapperHashTable.retrieve(aEntity);

        if (!entityWrapper)
            entityWrapper = new Entity(aEntity, this);
    }

    return entityWrapper;
}

/**
 * Create a wrapper for a nsIDOMNode, reuses an existing wrapper if possible.
 *
 * @param aNode the nsIDOMNode you want to wrap
 *
 * @return the Node
 */
Node* Document::createNode(nsIDOMNode* aNode)
{
    Node* nodeWrapper = NULL;

    if (aNode)
    {
        nodeWrapper = (Node*)wrapperHashTable.retrieve(aNode);

        if (!nodeWrapper)
            nodeWrapper = new Node(aNode, this);
    }

    return nodeWrapper;
}

/**
 * Create a wrapper for a nsIDOMNotation, reuses an existing wrapper if
 * possible.
 *
 * @param aNotation the nsIDOMNotation you want to wrap
 *
 * @return the Notation
 */
Notation* Document::createNotation(nsIDOMNotation* aNotation)
{
    Notation* notationWrapper = NULL;

    if (aNotation)
    {
        notationWrapper = (Notation*)wrapperHashTable.retrieve(aNotation);

        if (!notationWrapper)
            notationWrapper = new Notation(aNotation, this);
    }

    return notationWrapper;
}

/**
 * Create a wrapper for a nsIDOMDOMImplementation, reuses an existing wrapper if
 * possible.
 *
 * @param aImpl the nsIDOMDOMImplementation you want to wrap
 *
 * @return the DOMImplementation
 */
DOMImplementation* Document::createDOMImplementation(
            nsIDOMDOMImplementation* aImpl)
{
    DOMImplementation* implWrapper = NULL;

    if (aImpl)
    {
        implWrapper = (DOMImplementation*)wrapperHashTable.retrieve(aImpl);

        if (!implWrapper)
            implWrapper = new DOMImplementation(aImpl, this);
    }

    return implWrapper;
}

/**
 * Create a wrapper for a nsIDOMDocumentType, reuses an existing wrapper if
 * possible.
 *
 * @param aDoctype the nsIDOMDocumentType you want to wrap
 *
 * @return the DocumentType
 */
DocumentType* Document::createDocumentType(nsIDOMDocumentType* aDoctype)
{
    DocumentType* doctypeWrapper = NULL;

    if (aDoctype)
    {
        doctypeWrapper = (DocumentType*)wrapperHashTable.retrieve(aDoctype);

        if (!doctypeWrapper)
            doctypeWrapper = new DocumentType(aDoctype, this);
    }

    return doctypeWrapper;
}

/**
 * Create a wrapper for a nsIDOMNodeList, reuses an existing wrapper if
 * possible.
 *
 * @param aList the nsIDOMNodeList you want to wrap
 *
 * @return the NodeList
 */
NodeList* Document::createNodeList(nsIDOMNodeList* aList)
{
    NodeList* listWrapper = NULL;

    if (aList)
    {
        listWrapper = (NodeList*)wrapperHashTable.retrieve(aList);

        if (!listWrapper)
            listWrapper = new NodeList(aList, this);
    }

    return listWrapper;
}

/**
 * Create a wrapper for a nsIDOMNamedNodeMap, reuses an existing wrapper if
 * possible.
 *
 * @param aMap the nsIDOMNamedNodeMap you want to wrap
 *
 * @return the NamedNodeMap
 */
NamedNodeMap* Document::createNamedNodeMap(nsIDOMNamedNodeMap* aMap)
{
    NamedNodeMap* mapWrapper = NULL;

    if (aMap)
    {
        mapWrapper = (NamedNodeMap*)wrapperHashTable.retrieve(aMap);

        if (!mapWrapper)
            mapWrapper = new NamedNodeMap(aMap, this);
    }

    return mapWrapper;
}

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
            if (aNode == getKey())
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
 * @param aObj the MITREObject you want to add
 * @param aHashValue the key for the object in the hash table
 */
void Document::addWrapper(MITREObject* aObj, void* aHashValue)
{
    wrapperHashTable.add(aObj, aHashValue);
}

/**
 * Remove a wrapper from the document's hash table and return it to the caller.
 *
 * @param aHashValue the key for the object you want to remove
 *
 * @return the wrapper as a MITREObject
 */
MITREObject* Document::removeWrapper(void* aHashValue)
{
    return wrapperHashTable.remove(aHashValue);
}
