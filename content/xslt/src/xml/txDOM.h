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

#include "TxString.h"
#include "baseutils.h"
#ifndef NULL
typedef 0 NULL;
#endif

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
class DocumentType;

//
//Definition and Implementation the DOMImplementation class
//
class DOMImplementation
{
  public:
    DOMImplementation();
    ~DOMImplementation();

    MBool hasFeature(String feature, const String& version) const;

  private:
    String implFeature;
    String implVersion;
};

//
// Abstract Class defining the interface for a Node.  See NodeDefinition below
// for the actual implementation of the WC3 node.
//
class Node
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

    virtual ~Node() {}

    //Read functions
    virtual const String& getNodeName() const = 0;
    virtual const String& getNodeValue() const = 0;
    virtual const String& getNodeValue() = 0;
    virtual unsigned short getNodeType() const = 0;
    virtual Node* getParentNode() const = 0;
    virtual NodeList* getChildNodes() = 0;
    virtual Node* getFirstChild() const = 0;
    virtual Node* getLastChild() const = 0;
    virtual Node* getPreviousSibling() const = 0;
    virtual Node* getNextSibling() const = 0;
    virtual NamedNodeMap* getAttributes() = 0;
    virtual Document* getOwnerDocument() const = 0;

    //Write functions
    virtual void setNodeValue(const String& nodeValue) = 0;

    //Node manipulation functions
    virtual Node* insertBefore(Node* newChild, Node* refChild) = 0;
    virtual Node* replaceChild(Node* newChild, Node* oldChild) = 0;
    virtual Node* removeChild(Node* oldChild) = 0;
    virtual Node* appendChild(Node* newChild) = 0;
    virtual Node* cloneNode(MBool deep, Node* dest) = 0;

    virtual MBool hasChildNodes() const = 0;
};

//
// Abstract class containing the Interface for a NodeList.  See NodeDefinition
// below for the actual implementation of a WC3 NodeList as it applies to the
// getChildNodes Node function.  Also see NodeListDefinition for the
// implementation of a NodeList as it applies to such functions as
// getElementByTagName.
//
class NodeList
{
  public:
    virtual Node* item(Int32 index) = 0;
    virtual Int32 getLength() = 0;
  protected:
    Int32 length;
};

//
//Definition of the implementation of a NodeList.  This class maintains a
//linked list of pointers to Nodes.  "Friends" of the class can add and remove
//pointers to Nodes as needed.
//      *** NOTE: Is there any need for someone to "remove" a node from the
//                list?
//
class NodeListDefinition : public NodeList
{
  friend NamedNodeMap; //-- LF
  public:
    NodeListDefinition();
    ~NodeListDefinition();

    void append(Node& newNode);
    void append(Node* newNode);

    //Inherited from NodeList
    Node* item(Int32 index);
    Int32 getLength();

  protected:
    struct ListItem {
      ListItem* next;
      ListItem* prev;
      Node* node;
    };

    ListItem* firstItem;
    ListItem* lastItem;
};

//
//Definition of a NamedNodeMap.  For the time being it builds off the
//NodeListDefinition class.  This will probably change when NamedNodeMap needs
//to move to a more efficient search algorithm for attributes.
//
class NamedNodeMap : public NodeListDefinition
{
  public:
    NamedNodeMap();
    ~NamedNodeMap();

    Node* getNamedItem(const String& name);
    Node* setNamedItem(Node* arg);
    Node* removeNamedItem(const String& name);

  private:
    NodeListDefinition::ListItem* findListItemByName(const String& name);
};

//
// Definition and Implementation of Node and NodeList functionality.  This is
// the central class, from which all other DOM classes (objects) are derrived.
// Users of this DOM should work strictly with the Node interface and NodeList
// interface (see above for those definitions)
//
class NodeDefinition : public Node, public NodeList
{
  public:
    NodeDefinition(NodeType type, const String& name,
                   const String& value, Document* owner);
    virtual ~NodeDefinition();      //Destructor, delete all children of node

    //Read functions
    const String& getNodeName() const;
    virtual const String& getNodeValue() const;
    virtual const String& getNodeValue();
    unsigned short getNodeType() const;
    Node* getParentNode() const;
    NodeList* getChildNodes();
    Node* getFirstChild() const;
    Node* getLastChild() const;
    Node* getPreviousSibling() const;
    Node* getNextSibling() const;
    NamedNodeMap* getAttributes();
    Document* getOwnerDocument() const;

    //Write functions
    virtual void setNodeValue(const String& nodeValue);

    //Child node manipulation functions
    virtual Node* insertBefore(Node* newChild, Node* refChild);
    virtual Node* replaceChild(Node* newChild, Node* oldChild);
    virtual Node* removeChild(Node* oldChild);
    virtual Node* appendChild(Node* newChild);
    Node* cloneNode(MBool deep, Node* dest);

    MBool hasChildNodes() const;

    //Inherrited from NodeList
    Node* item(Int32 index);
    Int32 getLength();

  protected:
    //Name, value, and attributes for this node.  Available to derrived
    //classes, since those derrived classes have a better idea how to use them,
    //than the generic node does.
    String nodeName;
    String nodeValue;
    NamedNodeMap attributes;

    void DeleteChildren();

    Node* implInsertBefore(NodeDefinition* newChild, NodeDefinition* refChild);
  private:
    //Type of node this is
    NodeType nodeType;

    //Data members for linking this Node to its parent and siblings
    NodeDefinition* parentNode;
    NodeDefinition* previousSibling;
    NodeDefinition* nextSibling;

    //Pointer to the node's document
    Document* ownerDocument;

    //Data members for maintaining a list of child nodes
    NodeDefinition* firstChild;
    NodeDefinition* lastChild;

};

//
//Definition and Implementation of a Document Fragment.  All functionality is
//inherrited directly from NodeDefinition.  We just need to make sure the Type
//of the node set to Node::DOCUMENT_FRAGMENT_NODE.
//
class DocumentFragment : public NodeDefinition
{
  public:
    DocumentFragment(const String& name, const String& value, Document* owner);

    //Override insertBefore to limit Elements to having only certain nodes as
    //children
    Node* insertBefore(Node* newChild, Node* refChild);
};

//
//Definition and Implementation of a Document.
//
class Document : public NodeDefinition
{
  public:
    Document(DocumentType* theDoctype = NULL);

    Element* getDocumentElement();
    DocumentType* getDoctype();
    const DOMImplementation& getImplementation();

    //Factory functions for various node types
    DocumentFragment* createDocumentFragment();
    Element* createElement(const String& tagName);
    Attr* createAttribute(const String& name);
    Text* createTextNode(const String& theData);
    Comment* createComment(const String& theData);
    CDATASection* createCDATASection(const String& theData);
    ProcessingInstruction* createProcessingInstruction(const String& target,
                                                       const String& data);
    EntityReference* createEntityReference(const String& name);

    //Override functions to enforce the One Element rule for documents, as well
    //as limit documents to certain types of nodes.
    Node* insertBefore(Node* newChild, Node* refChild);
    Node* replaceChild(Node* newChild, Node* oldChild);
    Node* removeChild(Node* oldChild);

  private:
    Element* documentElement;
    DocumentType* doctype;
    DOMImplementation implementation;
};

//
//Definition and Implementation of an Element
//
class Element : public NodeDefinition
{
  public:
    Element(const String& tagName, Document* owner);

    //Override insertBefore to limit Elements to having only certain nodes as
    //children
    Node* insertBefore(Node* newChild, Node* refChild);

    const String& getTagName();
    const String& getAttribute(const String& name);
    void setAttribute(const String& name, const String& value);
    void removeAttribute(const String& name);
    Attr* getAttributeNode(const String& name);
    Attr* setAttributeNode(Attr* newAttr);
    Attr* removeAttributeNode(Attr* oldAttr);
    NodeList* getElementsByTagName(const String& name);
    void normalize();
};

//
//Definition and Implementation of a Attr
//    NOTE:  For the time bing use just the default functionality found in the
//           NodeDefinition class
//
class Attr : public NodeDefinition
{
  public:
    Attr(const String& name, Document* owner);

    const String& getName() const;
    MBool getSpecified() const;
    const String& getValue();
    void setValue(const String& newValue);

    //Override the set and get member functions for a node's value to create a
    //new TEXT node when set, and to interpret its children when read.
    void setNodeValue(const String& nodeValue);
    const String& getNodeValue();

    //Override insertBefore to limit Attr to having only certain nodes as
    //children
    Node* insertBefore(Node* newChild, Node* refChild);

  private:
    MBool specified;
};

//
//Definition and Implementation of CharacterData.  This class mearly provides
//the interface and some default implementation.  It is not intended to be
//instantiated by users of the DOM
//
class CharacterData : public NodeDefinition
{
  public:
    const String& getData() const;
    void setData(const String& source);
    Int32 getLength() const;

    String& substringData(Int32 offset, Int32 count, String& dest);
    void appendData(const String& arg);
    void insertData(Int32 offset, const String& arg);
    void deleteData(Int32 offset, Int32 count);
    void replaceData(Int32 offset, Int32 count, const String& arg);

  protected:
    CharacterData(NodeType type, const String& name,
                   const String& value, Document* owner);
};

//
//Definition and Implementation of a Text node.  The bulk of the functionality
//comes from CharacterData and NodeDefinition.
//
class Text : public CharacterData
{
  public:
    Text(const String& theData, Document* owner);

    Text* splitText(Int32 offset);

    //Override "child manipulation" function since Text Nodes can not have
    //any children.
    Node* insertBefore(Node* newChild, Node* refChild);
    Node* replaceChild(Node* newChild, Node* oldChild);
    Node* removeChild(Node* oldChild);
    Node* appendChild(Node* newChild);

  protected:
    Text(NodeType type, const String& name, const String& value,
         Document* owner);
};

//
//Definition and Implementation of a Comment node.  All of the functionality is
//inherrited from CharacterData and NodeDefinition.
//
class Comment : public CharacterData
{
  public:
    Comment(const String& theData, Document* owner);

    //Override "child manipulation" function since Comment Nodes can not have
    //any children.
    Node* insertBefore(Node* newChild, Node* refChild);
    Node* replaceChild(Node* newChild, Node* oldChild);
    Node* removeChild(Node* oldChild);
    Node* appendChild(Node* newChild);
};

//
//Definition and Implementation of a CDATASection node.  All of the
//functionality is inherrited from Text, CharacterData, and NodeDefinition
//
class CDATASection : public Text
{
  public:
    CDATASection(const String& theData, Document* owner);

    //Override "child manipulation" function since CDATASection Nodes can not
    //have any children.
    Node* insertBefore(Node* newChild, Node* refChild);
    Node* replaceChild(Node* newChild, Node* oldChild);
    Node* removeChild(Node* oldChild);
    Node* appendChild(Node* newChild);
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
    ProcessingInstruction(const String& theTarget, const String& theData,
                          Document* owner);

    const String& getTarget() const;
    const String& getData() const;

    void setData(const String& theData);

    //Override "child manipulation" function since ProcessingInstruction Nodes
    //can not have any children.
    Node* insertBefore(Node* newChild, Node* refChild);
    Node* replaceChild(Node* newChild, Node* oldChild);
    Node* removeChild(Node* oldChild);
    Node* appendChild(Node* newChild);
};

//
//Definition and Implementation of a Notation.  Most functionality is inherrited
//from NodeDefinition.
//
class Notation : public NodeDefinition
{
  public:
    Notation(const String& name, const String& pubID,
             const String& sysID);

    const String& getPublicId() const;
    const String& getSystemId() const;

    //Override "child manipulation" function since Notation Nodes
    //can not have any children.
    Node* insertBefore(Node* newChild, Node* refChild);
    Node* replaceChild(Node* newChild, Node* oldChild);
    Node* removeChild(Node* oldChild);
    Node* appendChild(Node* newChild);

  private:
    String publicId;
    String systemId;
};

//
//Definition and Implementation of an Entity
//
class Entity : public NodeDefinition
{
  public:
    Entity(const String& name, const String& pubID,
           const String& sysID, const String& notName);

    const String& getPublicId() const;
    const String& getSystemId() const;
    const String& getNotationName() const;

    //Override insertBefore to limit Entity to having only certain nodes as
    //children
    Node* insertBefore(Node* newChild, Node* refChild);

  private:
    String publicId;
    String systemId;
    String notationName;
};

//
//Definition and Implementation of an EntityReference
//
class EntityReference : public NodeDefinition
{
  public:
    EntityReference(const String& name, Document* owner);

    //Override insertBefore to limit EntityReference to having only certain
    //nodes as children
    Node* insertBefore(Node* newChild, Node* refChild);
};

//
//Definition and Implementation of the DocumentType
//
class DocumentType : public NodeDefinition
{
  public:
    DocumentType(const String& name, NamedNodeMap* theEntities,
                 NamedNodeMap* theNotations);
    ~DocumentType();

    NamedNodeMap* getEntities();
    NamedNodeMap* getNotations();

    //Override "child manipulation" function since Notation Nodes
    //can not have any children.
    Node* insertBefore(Node* newChild, Node* refChild);
    Node* replaceChild(Node* newChild, Node* oldChild);
    Node* removeChild(Node* oldChild);
    Node* appendChild(Node* newChild);

  private:
    NamedNodeMap* entities;
    NamedNodeMap* notations;
};

//NULL string for use by Element::getAttribute() for when the attribute
//spcified by "name" does not exist, and therefore shoud be "NULL".
const String NULL_STRING;

#endif
