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
// Tom Kneeland (01/13/2000) 
// 
//  Implementation of the Mozilla Wrapper classes for the Mozilla DOM to 
//  TransforMIIX DOM interface conversion.  Note that these wrapper classes are 
//  only interchangable with the TransforMIIX DOM at compile time.  They are 
//  merely a copy of the TransforMIIX interface/class structure wich then 
//  deffer their processing to a Mozilla object.  Complete interchangability 
//  would require the use of multiple inherritance using virtual base classes 
//  and RTTI which is not supported in Mozilla's cross platform domain. 
// 
//  NOTE:  The objects being wrapped are not deleted.  It is assumed that they 
//         will be deleted when the actual ("real") Mozilla Document is 
//         destroyed 
// 
//         Also note that the Document wrapper class maintains a hash table of 
//         wrapper object.  The objects are hashed based on the address of the 
//         mozilla object they are wrapping.  When ever a new wrapper object is 
//         created, the Hash Table should first be check to see if one already 
//         exists. 
// 
// Modification History: 
// Who  When        What 
// TK   02/11/2000  Added a default constructor for the Document class.  Simply 
//                  create an nsXMLDocument object is one is not provided. 
 
#ifndef MOZILLA_MITRE_DOM
#define MOZILLA_MITRE_DOM

#ifdef __BORLANDC__ 
#include <stdlib.h> 
#endif 

#include "MozillaString.h" 
#include "baseutils.h" 
#include "HashTable.h" 
 
//A bunch of Mozilla DOM headers 
#include "nsIDOMNode.h" 
#include "nsIDOMDocument.h" 
#include "nsIDOMElement.h" 
#include "nsIDOMProcessingInstruction.h" 
#include "nsIDOMAttr.h" 
#include "nsIDOMCDATASection.h" 
#include "nsIDOMText.h" 
#include "nsIDOMDOMImplementation.h" 
#include "nsIDOMDocumentType.h" 
#include "nsIDOMEntityReference.h" 
#include "nsIDOMDocumentFragment.h" 
#include "nsIDOMComment.h" 
#include "nsIDOMNodeList.h" 
#include "nsIDOMNotation.h" 
#include "nsIDOMEntity.h" 
#include "nsIDOMNamedNodeMap.h" 
#include "nsIDOMCharacterData.h" 

#ifndef NULL 
typedef 0 NULL; 
#endif 
 
 
typedef MozillaString DOMString; 
typedef UNICODE_CHAR DOM_CHAR; 
 
class NodeList; 
class NamedNodeMap; 
class Document; 
class Element; 
class Attr; 
class Text; 
class Comment; 
class CDATASection; 
class ProcessingInstruction; 
class EntityReference; 
class Entity; 
class DocumentType; 
class Notation; 
 
// 
//Wrapper class for nsIDOMDOMImplementation.  Convert to TransforMIIX 
//DOMImplementation interface. 
// 
class DOMImplementation : public MITREObject 
{ 
  public: 
    DOMImplementation(nsIDOMDOMImplementation* domImpl, Document* owner); 
    ~DOMImplementation(); 
 
    void setNSObj(nsIDOMDOMImplementation* domImpl); 
 
    virtual MBool hasFeature(const DOMString& feature, const DOMString& version) const; 
 
  private: 
    nsIDOMDOMImplementation* nsDOMImpl; 
 
    Document* ownerDocument; 
}; 
 
// 
// Definition and Implementation of the Node Wrapper class.  It provides the 
// generic Node interface, forwarding all functionality to its Mozilla 
// counterpart. 
// 
class Node : public MITREObject 
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
 
    Node(); 
    Node(nsIDOMNode* node, Document* owner); 
    virtual ~Node(); 
 
    virtual void setNSObj(nsIDOMNode* node); 
    void setNSObj(nsIDOMNode* node, Document* owner); 
    nsIDOMNode* getNSObj(); 
 
    //Read functions 
    virtual const DOMString& getNodeName(); 
    virtual const DOMString& getNodeValue(); 
    virtual unsigned short getNodeType() const; 
    virtual Node* getParentNode(); 
    virtual NodeList* getChildNodes(); 
    virtual Node* getFirstChild(); 
    virtual Node* getLastChild(); 
    virtual Node* getPreviousSibling(); 
    virtual Node* getNextSibling(); 
    virtual NamedNodeMap* getAttributes(); 
    virtual Document* getOwnerDocument(); 
 
    //Write functions 
    virtual void setNodeValue(const DOMString& nodeValue); 
 
    //Node manipulation functions 
    virtual Node* insertBefore(Node* newChild, Node* refChild); 
    virtual Node* replaceChild(Node* newChild, Node* oldChild); 
    virtual Node* removeChild(Node* oldChild); 
    virtual Node* appendChild(Node* newChild); 
    virtual Node* cloneNode(MBool deep, Node* dest); 
 
    virtual MBool hasChildNodes() const; 
 
  protected: 
    //We want to maintain a pointer back to the owner document for Hash/ 
    //memory management. 
    Document* ownerDocument; 
 
  private: 
    nsIDOMNode* nsNode; 
 
    //Maintain a few DOMString objects to ensure proper handling of references 
    DOMString nodeName; 
    DOMString nodeValue; 
}; 
 
// 
//Wrapper class for nsIDOMNodeList.  Convert to TransforMIIX 
//NodeList interface. 
// 
class NodeList : public MITREObject 
{ 
  public: 
    NodeList(nsIDOMNodeList* nodeList, Document* owner); 
    ~NodeList(); 
 
    Node* item(UInt32 index); 
    UInt32 getLength(); 
  protected: 
    nsIDOMNodeList* nsNodeList; 
 
    Document* ownerDocument; 
}; 
 
 
// 
//Wrapper class for nsIDOMNamedNodeMap.  Convert to TransforMIIX 
//NameNodeMap interface. 
// 
class NamedNodeMap : public MITREObject 
{ 
  public: 
    NamedNodeMap(nsIDOMNamedNodeMap* namedNodeMap, Document* owner); 
    ~NamedNodeMap(); 
 
    Node* getNamedItem(const DOMString& name); 
    Node* setNamedItem(Node* arg); 
    Node* removeNamedItem(const DOMString& name); 
    Node* item(UInt32 index); 
    UInt32 getLength(); 
 
  private: 
    nsIDOMNamedNodeMap* nsNamedNodeMap; 
    Document* ownerDocument; 
}; 
 
// 
//Wrapper class for nsIDOMDocumentFragment.  Convert to TransforMIIX 
//DocumentFragment interface. 
// 
class DocumentFragment : public Node 
{ 
  public: 
    DocumentFragment(nsIDOMDocumentFragment* docFragment, Document* owner); 
    ~DocumentFragment(); 
 
    void setNSObj(nsIDOMDocumentFragment* docFragment); 
 
    //TK - Just deffer to the Node Implementation since it already has a pointer 
    //     to the proper "polymorphed" object. 
    //Override insertBefore to limit Elements to having only certain nodes as 
    //children 
    //Node* insertBefore(Node* newChild, Node* refChild); 
 
  private: 
    nsIDOMDocumentFragment* nsDocumentFragment; 
}; 
 
// 
//Wrapper class for nsIDOMDocument.  Convert to TransforMIIX 
//Document interface. 
// 
class Document : public Node 
{ 
  public: 
    Document(); 
    Document(nsIDOMDocument* document); 
    ~Document(); 
 
    virtual void setNSObj(nsIDOMDocument* node); 
 
    Element* getDocumentElement(); 
    DocumentType* getDoctype(); 
    DOMImplementation* getImplementation(); 
 
    //Provide a means to remove items from the wrapperHashTable 
    MITREObject* removeWrapper(Int32 hashValue); 
     
    //Provide a means to place a specific wrapper in the hash table 
    //This is needed to support the concept of "cloning" a node where 
    //a precreated destination wrapper is provided as a destination. 
    //We need to be able to remove the provided wrapper (above) so a new 
    //object can be associated with it, then placed back in the hash 
    void addWrapper(MITREObject* obj, Int32 hashValue); 
 
    //Factory functions for various node types.  These functions 
    //are responsible for storing the wrapper classes they create in  
    //the document's wrapperHashTable. 
    //Note the addition of the factory functions to "wrap" 
    //nsIDOM* objects. 
    DocumentFragment* createDocumentFragment(); 
    DocumentFragment* createDocumentFragment(nsIDOMDocumentFragment* fragment); 
     
    Element* createElement(const DOMString& tagName); 
    Element* createElement(nsIDOMElement* element); 
     
    Attr* createAttribute(const DOMString& name); 
    Attr* createAttribute(nsIDOMAttr* attr); 
 
    Text* createTextNode(const DOMString& theData); 
    Text* createTextNode(nsIDOMText* text); 
 
    Comment* createComment(const DOMString& theData); 
    Comment* createComment(nsIDOMComment* comment); 
 
    CDATASection* createCDATASection(const DOMString& theData); 
    CDATASection* createCDATASection(nsIDOMCDATASection* cdata); 
 
    ProcessingInstruction* createProcessingInstruction(const DOMString& target, 
                                                       const DOMString& data); 
    ProcessingInstruction* createProcessingInstruction( 
    			      nsIDOMProcessingInstruction* pi); 
 
    EntityReference* createEntityReference(const DOMString& name); 
    EntityReference* createEntityReference(nsIDOMEntityReference* entiyRef); 
 
    Entity* createEntity(nsIDOMEntity* entity); 
 
    Node* createNode(nsIDOMNode* node); 
    DOMString* createDOMString(nsString* str); 
    Notation* createNotation(nsIDOMNotation* notation); 
    DOMImplementation* createDOMImplementation(nsIDOMDOMImplementation* impl); 
    DocumentType* createDocumentType(nsIDOMDocumentType* doctype); 
    NodeList* createNodeList(nsIDOMNodeList* list); 
    NamedNodeMap* createNamedNodeMap(nsIDOMNamedNodeMap* map); 
 
    //Determine what kind of node this is, and create the appropriate wrapper 
    //for it. 
    Node* createWrapper(nsIDOMNode* node); 
 
    //TK - Just deffer to the Node implementation 
    // 
    //Override functions to enforce the One Element rule for documents, as well 
    //as limit documents to certain types of nodes. 
    //Node* insertBefore(Node* newChild, Node* refChild); 
    //Node* replaceChild(Node* newChild, Node* oldChild); 
    //Node* removeChild(Node* oldChild); 
 
  private: 
    nsIDOMDocument* nsDocument; 
 
    //Hash tables to maintain wrapper objects.   
    //NOTE:  Should we maintain separate hash table for NamedNodeMaps and 
    //       NodeLists? 
    HashTable wrapperHashTable; 
}; 
 
// 
//Wrapper class for nsIDOMElement.  Convert to TransforMIIX 
//Element interface. 
// 
class Element : public Node 
{ 
  public: 
  //Element(); 
    Element(nsIDOMElement* element, Document* owner); 
    ~Element(); 
 
    virtual void setNSObj(nsIDOMElement* element); 
 
    //TK - Just deffer to the Node Implementation 
    // 
    //Override insertBefore to limit Elements to having only certain nodes as 
    //children 
    //Node* insertBefore(Node* newChild, Node* refChild); 
 
    const DOMString& getTagName(); 
    const DOMString& getAttribute(const DOMString& name); 
    void setAttribute(const DOMString& name, const DOMString& value); 
    void removeAttribute(const DOMString& name); 
    Attr* getAttributeNode(const DOMString& name); 
    Attr* setAttributeNode(Attr* newAttr); 
    Attr* removeAttributeNode(Attr* oldAttr); 
    NodeList* getElementsByTagName(const DOMString& name); 
    void normalize(); 
 
  private: 
    nsIDOMElement* nsElement; 
}; 
 
// 
//Wrapper class for nsIDOMAttr.  Convert to TransforMIIX 
//Attr interface. 
// 
class Attr : public Node 
{ 
  public: 
    Attr(nsIDOMAttr* attr, Document* owner); 
    ~Attr(); 
 
    nsIDOMAttr* getNSAttr(); 
    virtual void setNSObj(nsIDOMAttr* attr); 
 
    const DOMString& getName(); 
    MBool getSpecified() const; 
    const DOMString& getValue(); 
    void setValue(const DOMString& newValue); 
 
    //TK - Simply deffer to Node Implementation 
    //Override the set and get member functions for a node's value to create a 
    //new TEXT node when set, and to interpret its children when read. 
    //void setNodeValue(const DOMString& nodeValue); 
    //const DOMString& getNodeValue(); 
 
    //Override insertBefore to limit Attr to having only certain nodes as 
    //children 
    //Node* insertBefore(Node* newChild, Node* refChild); 
 
  private: 
    // 
    //I don't think we need these since the MozillaString objects will 
    //delete their nsString objects when they are destroyed. 
    // 
    //We don't want to make any assumptions about the implementation of 
    //the NS attribute object, so keep these around to store the name and 
    //value of the attribute when requested by the user. 
    //We don't want to make any assumptions about the implementation of 
    //DOMString attrName; 
    //DOMString attrValue; 
 
    nsIDOMAttr* nsAttr; 
}; 
 
// 
//Wrapper class for nsIDOMCharacterData.  Convert to TransforMIIX 
//CharacterData interface. 
// 
class CharacterData : public Node 
{ 
  public: 
    virtual ~CharacterData(); 
 
    virtual void setNSObj(nsIDOMCharacterData* charData); 
 
    const DOMString& getData() const; 
    void setData(const DOMString& source); 
    Int32 getLength() const; 
 
    DOMString& substringData(Int32 offset, Int32 count, DOMString& dest); 
    void appendData(const DOMString& arg); 
    void insertData(Int32 offset, const DOMString& arg); 
    void deleteData(Int32 offset, Int32 count); 
    void replaceData(Int32 offset, Int32 count, const DOMString& arg); 
 
  protected: 
    CharacterData(nsIDOMCharacterData* charData, Document* owner); 
 
  private: 
    nsIDOMCharacterData* nsCharacterData; 
}; 
 
// 
//Wrapper class for nsIDOMText.  Convert to TransforMIIX 
//Text interface. 
// 
class Text : public CharacterData 
{ 
  public: 
    Text(nsIDOMText* text, Document* owner); 
    ~Text(); 
 
    virtual void setNSObj(nsIDOMText* text); 
 
    Text* splitText(Int32 offset); 
 
    //TK - simply deffer to Node implementation 
    //Override "child manipulation" function since Text Nodes can not have 
    //any children. 
    //Node* insertBefore(Node* newChild, Node* refChild); 
    //Node* replaceChild(Node* newChild, Node* oldChild); 
    //Node* removeChild(Node* oldChild); 
    //Node* appendChild(Node* newChild); 
 
  private: 
    nsIDOMText* nsText; 
}; 
 
// 
//Wrapper class for nsIDOMComment.  Convert to TransforMIIX 
//Comment interface. 
// 
class Comment : public CharacterData 
{ 
  public: 
    Comment(nsIDOMComment* comment, Document* owner); 
    ~Comment(); 
 
    virtual void setNSObj(nsIDOMComment* comment); 
 
    //TK - Simply defer to Node Implementation 
    //Override "child manipulation" function since Comment Nodes can not have 
    //any children. 
    //Node* insertBefore(Node* newChild, Node* refChild); 
    //Node* replaceChild(Node* newChild, Node* oldChild); 
    //Node* removeChild(Node* oldChild); 
    //Node* appendChild(Node* newChild); 
 
  private: 
    nsIDOMComment* nsComment; 
}; 
 
// 
//Wrapper class for nsIDOMCDATASection.  Convert to TransforMIIX 
//CDATASection interface. 
// 
class CDATASection : public Text 
{ 
  public: 
    CDATASection(nsIDOMCDATASection* cdataSection, Document* owner); 
    ~CDATASection(); 
 
    virtual void setNSObj(nsIDOMCDATASection* cdataSection); 
 
    //TK - Simply defer to Node Implementation 
    //Override "child manipulation" function since CDATASection Nodes can not 
    //have any children. 
    //Node* insertBefore(Node* newChild, Node* refChild); 
    //Node* replaceChild(Node* newChild, Node* oldChild); 
    //Node* removeChild(Node* oldChild); 
    //Node* appendChild(Node* newChild); 
 
  private: 
    nsIDOMCDATASection* nsCDATASection; 
}; 
 
// 
//Wrapper class for nsIDOMProcessingInstruction.  Convert to TransforMIIX 
//ProcessingInstruction interface. 
// 
class ProcessingInstruction : public Node 
{ 
  public: 
    ProcessingInstruction(nsIDOMProcessingInstruction* procInstr,  
			  Document* owner); 
    ~ProcessingInstruction(); 
 
    virtual void setNSObj(nsIDOMProcessingInstruction* procInstr); 
 
    const DOMString& getTarget() const; 
    const DOMString& getData() const; 
 
    void setData(const DOMString& theData); 
 
    //TK - Defer to Node Implementation 
    //Override "child manipulation" function since ProcessingInstruction Nodes 
    //can not have any children. 
    //Node* insertBefore(Node* newChild, Node* refChild); 
    //Node* replaceChild(Node* newChild, Node* oldChild); 
    //Node* removeChild(Node* oldChild); 
    //Node* appendChild(Node* newChild); 
 
  private: 
    nsIDOMProcessingInstruction* nsProcessingInstruction; 
}; 
 
// 
//Wrapper class for nsIDOMNotation.  Convert to TransforMIIX 
//Notation interface. 
// 
class Notation : public Node 
{ 
  public: 
    Notation(nsIDOMNotation* notation, Document* owner); 
    ~Notation(); 
 
    virtual void setNSObj(nsIDOMNotation* notation); 
 
    const DOMString& getPublicId() const; 
    const DOMString& getSystemId() const; 
 
    //TK - Simply Defer to Node Implemntation 
    //Override "child manipulation" function since Notation Nodes 
    //can not have any children. 
    //Node* insertBefore(Node* newChild, Node* refChild); 
    //Node* replaceChild(Node* newChild, Node* oldChild); 
    //Node* removeChild(Node* oldChild); 
    //Node* appendChild(Node* newChild); 
 
  private: 
    nsIDOMNotation* nsNotation; 
}; 
 
// 
//Wrapper class for nsIDOMEntity.  Convert to TransforMIIX 
//Entity interface. 
// 
class Entity : public Node 
{ 
  public: 
    Entity(nsIDOMEntity* entity, Document* owner); 
    ~Entity(); 
 
    virtual void setNSObj(nsIDOMEntity* entity); 
 
    const DOMString& getPublicId() const; 
    const DOMString& getSystemId() const; 
    const DOMString& getNotationName() const; 
 
    //TK - Defer to Node Implementation 
    //Override insertBefore to limit Entity to having only certain nodes as 
    //children 
    //Node* insertBefore(Node* newChild, Node* refChild); 
 
  private: 
    nsIDOMEntity* nsEntity; 
}; 
 
// 
//Wrapper class for nsIDOMEntityReference.  Convert to TransforMIIX 
//EntityReference interface. 
// 
class EntityReference : public Node 
{ 
  public: 
    EntityReference(nsIDOMEntityReference* entityReference, Document* owner); 
    ~EntityReference(); 
 
    virtual void setNSObj(nsIDOMEntityReference* entityReference); 
 
    //TK - Simply defer to Node Implementation 
    //Override insertBefore to limit EntityReference to having only certain 
    //nodes as children 
    //Node* insertBefore(Node* newChild, Node* refChild); 
 
  private: 
    nsIDOMEntityReference* nsEntityReference; 
}; 
 
// 
//Wrapper class for nsIDOMDocumentType.  Convert to TransforMIIX 
//Document interface. 
// 
class DocumentType : public Node 
{ 
  public: 
    DocumentType(); 
    DocumentType(nsIDOMDocumentType* documentType, Document* owner); 
    ~DocumentType(); 
 
    void setNSObj(nsIDOMDocumentType* documentType); 
 
    const DOMString& getName() const; 
    NamedNodeMap* getEntities(); 
    NamedNodeMap* getNotations(); 
 
    //TK - Simply deffer to Node Implementation 
    //Override "child manipulation" function since Notation Nodes 
    //can not have any children. 
    //Node* insertBefore(Node* newChild, Node* refChild); 
    //Node* replaceChild(Node* newChild, Node* oldChild); 
    //Node* removeChild(Node* oldChild); 
    //Node* appendChild(Node* newChild); 
 
  private: 
    nsIDOMDocumentType* nsDocumentType; 
}; 
 
//NULL string for use by Element::getAttribute() for when the attribute 
//spcified by "name" does not exist, and therefore shoud be "NULL". 
const DOMString NULL_STRING; 
 
#endif 
