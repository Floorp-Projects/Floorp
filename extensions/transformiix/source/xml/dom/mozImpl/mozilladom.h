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

/* Definition of the wrapper classes.
*/

/**
 * Implementation of the Mozilla Wrapper classes for the Mozilla DOM to
 * TransforMIIX DOM interface conversion. Note that these wrapper classes are
 * only interchangable with the TransforMIIX DOM at compile time. They are
 * merely a copy of the TransforMIIX interface/class structure wich then
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

#ifdef __BORLANDC__
#include <stdlib.h>
#endif

#include "TxString.h"
#include "HashTable.h"

//A bunch of Mozilla DOM headers
#include "nsCOMPtr.h"

#include "nsIDocument.h"

#include "nsIDOMAttr.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMComment.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMElement.h"
#include "nsIDOMEntity.h"
#include "nsIDOMEntityReference.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNotation.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMText.h"

#ifndef NULL
typedef 0 NULL;
#endif

typedef UNICODE_CHAR DOM_CHAR;

class Attr;
class CDATASection;
class Comment;
class Document;
class DocumentType;
class Element;
class Entity;
class EntityReference;
class NamedNodeMap;
class NodeList;
class Notation;
class ProcessingInstruction;
class Text;

/**
 * Base wrapper class for a Mozilla object. Owns the Mozilla object through an
 * nsCOMPtr<nsISupports>.
 */
class MozillaObjectWrapper : public MITREObject
{
    public:
        MozillaObjectWrapper(nsISupports* aNsObject, Document* aaOwner);
        ~MozillaObjectWrapper();

        void setNSObj(nsISupports* aNsObject);
        void setNSObj(nsISupports* aNsObject, Document* aaOwner);

        void* getKey() const;
   
    protected:
        // We want to maintain a pointer back to the aOwner document for memory
        // management.
        Document* ownerDocument;

    private:
        nsCOMPtr<nsISupports> nsObject;
};

/**
 * Wrapper class for nsIDOMDOMImplementation.
 */
class DOMImplementation : public MozillaObjectWrapper
{
    public:
        DOMImplementation(nsIDOMDOMImplementation* aDomImpl, Document* aaOwner);
        ~DOMImplementation();

        void setNSObj(nsIDOMDOMImplementation* aDomImpl);

        MBool hasFeature(const String& aFeature, const String& aVersion) const;

    private:
        nsIDOMDOMImplementation* nsDOMImpl;
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

        Node();
        Node(nsIDOMNode* aNode, Document* aOwner);
        virtual ~Node();

        void setNSObj(nsIDOMNode* aNode);
        void setNSObj(nsIDOMNode* aNode, Document* aOwner);
        nsIDOMNode* getNSObj();

        // Read functions
        virtual const String& getNodeName();
        virtual const String& getNodeValue();
        virtual unsigned short getNodeType() const;
        virtual Node* getParentNode();
        virtual NodeList* getChildNodes();
        virtual Node* getFirstChild();
        virtual Node* getLastChild();
        virtual Node* getPreviousSibling();
        virtual Node* getNextSibling();
        virtual NamedNodeMap* getAttributes();
        virtual Document* getOwnerDocument();

        // Write functions
        virtual void setNodeValue(const String& aNodeValue);

        // Node manipulation functions
        virtual Node* insertBefore(Node* aNewChild, Node* aRefChild);
        virtual Node* replaceChild(Node* aNewChild, Node* aOldChild);
        virtual Node* removeChild(Node* aOldChild);
        virtual Node* appendChild(Node* aNewChild);
        virtual Node* cloneNode(MBool aDeep, Node* aDest);

        virtual MBool hasChildNodes() const;

    protected:
        String nodeName;
        String nodeValue;

    private:
        nsIDOMNode* nsNode;
};

/**
 * Wrapper class for nsIDOMNodeList.
 */
class NodeList : public MozillaObjectWrapper
{
    public:
        NodeList(nsIDOMNodeList* aNodeList, Document* aOwner);
        ~NodeList();

        void setNSObj(nsIDOMNodeList* aNodeList);

        Node* item(UInt32 aIndex);
        UInt32 getLength();

    protected:
        nsIDOMNodeList* nsNodeList;
};


/**
 * Wrapper class for nsIDOMNamedNodeMap.
 */
class NamedNodeMap : public MozillaObjectWrapper
{
    public:
        NamedNodeMap(nsIDOMNamedNodeMap* aNamedNodeMap, Document* aOwner);
        ~NamedNodeMap();

        void setNSObj(nsIDOMNamedNodeMap* aNamedNodeMap);

        Node* getNamedItem(const String& aName);
        Node* setNamedItem(Node* aNode);
        Node* removeNamedItem(const String& aName);
        Node* item(UInt32 aIndex);
        UInt32 getLength();

    private:
        nsIDOMNamedNodeMap* nsNamedNodeMap;
};

/**
 * Wrapper class for nsIDOMDocumentFragment.
 */
class DocumentFragment : public Node
{
    public:
        DocumentFragment(nsIDOMDocumentFragment* aDocFragment,
                    Document* aOwner);
        ~DocumentFragment();

        void setNSObj(nsIDOMDocumentFragment* aDocFragment);

    private:
        nsIDOMDocumentFragment* nsDocumentFragment;
};

/**
 * Wrapper class for nsIDOMDocument.
 */
class Document : public Node
{
    public:
        Document();
        Document(nsIDOMDocument* aDocument);
        ~Document();

        void setNSObj(nsIDOMDocument* aNode);

        PRBool inHashTableDeletion();

        Element* getDocumentElement();
        DocumentType* getDoctype();
        DOMImplementation* getImplementation();

        // Determine what kind of node this is, and create the appropriate
        // wrapper for it.
        Node* createWrapper(nsIDOMNode* node);
        void addWrapper(MITREObject* aObj, void* aHashValue);
        MITREObject* removeWrapper(void* aHashValue);

        // Factory functions for various node types.  These functions
        // are responsible for storing the wrapper classes they create in 
        // the document's wrapperHashTable.
        // Note the addition of the factory functions to "wrap"
        // nsIDOM* objects.
        DocumentFragment* createDocumentFragment();
        DocumentFragment* createDocumentFragment(
                    nsIDOMDocumentFragment* aFragment);
    
        Element* createElement(const String& aTagName);
        Element* createElement(nsIDOMElement* aElement);
    
        Element* createElementNS(const String& aNamespaceURI,
                    const String& aTagName);

        Attr* createAttribute(const String& aName);
        Attr* createAttribute(nsIDOMAttr* aAttr);

        Text* createTextNode(const String& aData);
        Text* createTextNode(nsIDOMText* aText);

        Comment* createComment(const String& aData);
        Comment* createComment(nsIDOMComment* aComment);

        CDATASection* createCDATASection(const String& aData);
        CDATASection* createCDATASection(nsIDOMCDATASection* aCdata);

        ProcessingInstruction* createProcessingInstruction(
                    const String& aTarget, const String& aData);
        ProcessingInstruction* createProcessingInstruction(
                    nsIDOMProcessingInstruction* aPi);

        EntityReference* createEntityReference(const String& aName);
        EntityReference* createEntityReference(
                    nsIDOMEntityReference* aEntiyRef);

        Entity* createEntity(nsIDOMEntity* aEntity);

        Node* createNode(nsIDOMNode* aNode);

        Notation* createNotation(nsIDOMNotation* aNotation);

        DOMImplementation* createDOMImplementation(
                    nsIDOMDOMImplementation* aImpl);

        DocumentType* createDocumentType(nsIDOMDocumentType* aDoctype);

        NodeList* createNodeList(nsIDOMNodeList* aList);
 
        NamedNodeMap* createNamedNodeMap(nsIDOMNamedNodeMap* aMap);

    private:
        nsIDOMDocument* nsDocument;

        PRBool bInHashTableDeletion;

        HashTable wrapperHashTable;
};

/**
 * Wrapper class for nsIDOMElement.
 */
class Element : public Node
{
    public:
        Element(nsIDOMElement* aElement, Document* aOwner);
        ~Element();

        void setNSObj(nsIDOMElement* aElement);

        const String& getTagName();
        const String& getAttribute(const String& aName);
        void setAttribute(const String& aName, const String& aValue);
        void setAttributeNS(const String& aNamespaceURI, const String& aName,
                    const String& aValue);
        void removeAttribute(const String& aName);
        Attr* getAttributeNode(const String& aName);
        Attr* setAttributeNode(Attr* aNewAttr);
        Attr* removeAttributeNode(Attr* aOldAttr);
        NodeList* getElementsByTagName(const String& aName);
        void normalize();

    private:
        nsIDOMElement* nsElement;
};

/**
 * Wrapper class for nsIDOMAttr.
 */
class Attr : public Node
{
    public:
        Attr(nsIDOMAttr* aAttr, Document* aOwner);
        ~Attr();

        nsIDOMAttr* getNSAttr();
        void setNSObj(nsIDOMAttr* aAttr);

        const String& getName();
        MBool getSpecified() const;
        const String& getValue();
        void setValue(const String& aNewValue);

    private:
        nsIDOMAttr* nsAttr;
};

/**
 * Wrapper class for nsIDOMCharacterData.
 */
class CharacterData : public Node
{
    public:
        CharacterData(nsIDOMCharacterData* aCharData, Document* aOwner);
        ~CharacterData();

        void setNSObj(nsIDOMCharacterData* aCharData);

        const String& getData();
        void setData(const String& aSource);
        Int32 getLength() const;

        String& substringData(Int32 aOffset, Int32 aCount, String& aDest);
        void appendData(const String& aSource);
        void insertData(Int32 aOffset, const String& aSource);
        void deleteData(Int32 aOffset, Int32 aCount);
        void replaceData(Int32 aOffset, Int32 aCount, const String& aSource);

    private:
        nsIDOMCharacterData* nsCharacterData;
};

/**
 * Wrapper class for nsIDOMText.
 */
class Text : public CharacterData
{
    public:
        Text(nsIDOMText* aText, Document* aOwner);
        ~Text();

        void setNSObj(nsIDOMText* aText);

        Text* splitText(Int32 aOffset);

    private:
        nsIDOMText* nsText;
};

/**
 * Wrapper class for nsIDOMComment.
 */
class Comment : public CharacterData
{
    public:
        Comment(nsIDOMComment* aComment, Document* aOwner);
        ~Comment();

        void setNSObj(nsIDOMComment* aComment);

    private:
        nsIDOMComment* nsComment;
};

/**
 * Wrapper class for nsIDOMCDATASection.
 */
class CDATASection : public Text
{
    public:
        CDATASection(nsIDOMCDATASection* aCdataSection, Document* aOwner);
        ~CDATASection();

        void setNSObj(nsIDOMCDATASection* aCdataSection);

    private:
        nsIDOMCDATASection* nsCDATASection;
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

        void setNSObj(nsIDOMProcessingInstruction* aProcInstr);

        const String& getTarget();
        const String& getData();

        void setData(const String& aData);

    private:
        nsIDOMProcessingInstruction* nsProcessingInstruction;

        String target;
        String data;
};

/**
 * Wrapper class for nsIDOMNotation.
 */
class Notation : public Node
{
    public:
        Notation(nsIDOMNotation* aNotation, Document* aOwner);
        ~Notation();

        void setNSObj(nsIDOMNotation* aNotation);

        const String& getPublicId();
        const String& getSystemId();

    private:
        nsIDOMNotation* nsNotation;

        String publicId;
        String systemId;
};

/**
 * Wrapper class for nsIDOMEntity.
 */
class Entity : public Node
{
    public:
        Entity(nsIDOMEntity* aEntity, Document* aOwner);
        ~Entity();

        void setNSObj(nsIDOMEntity* aEntity);

        const String& getPublicId();
        const String& getSystemId();
        const String& getNotationName();

    private:
        nsIDOMEntity* nsEntity;

        String publicId;
        String systemId;
        String notationName;
};

/**
 * Wrapper class for nsIDOMEntityReference.
 */
class EntityReference : public Node
{
    public:
        EntityReference(nsIDOMEntityReference* aEntityReference,
                    Document* aOwner);
        ~EntityReference();

        void setNSObj(nsIDOMEntityReference* aEntityReference);

    private:
        nsIDOMEntityReference* nsEntityReference;
};

/**
 * Wrapper class for nsIDOMDocumentType.
 */
class DocumentType : public Node
{
    public:
        DocumentType();
        DocumentType(nsIDOMDocumentType* aDocumentType, Document* aOwner);
        ~DocumentType();

        void setNSObj(nsIDOMDocumentType* aDocumentType);

        const String& getName();
        NamedNodeMap* getEntities();
        NamedNodeMap* getNotations();

    private:
       nsIDOMDocumentType* nsDocumentType;
};

// NULL string for use by Element::getAttribute() for when the attribute
// specified by "name" does not exist, and therefore should be "NULL".
const String NULL_STRING;

#endif
