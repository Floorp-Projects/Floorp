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
// Tom Kneeland (01/18/2000) 
// 
//  Implementation of the Mozilla Wrapper class for the Document Object. 
// 
// Modification History: 
// Who  When        What 
// TK   02/11/2000  Added default constructor.  If no NS object is provided 
//                  simply create an nsXMLDocument to use. 
// TK   02/15/2000  Fixed bug in Create Wrapper.  We must check to see if the 
//                  the nsIDOM* object is NULL.  If it is, then we don't need 
//                  a wrapper. 
//                  Also fixed the same problem with the other factory methods 
//                  that create wrappers for Mozilla objects. 
// 
 
#include "mozilladom.h" 
#include "iostream.h" 
#include "nsCOMPtr.h" 
#include "nsIContent.h" 
 
// 
//Construct a Document Wrapper object without specificy a nsIDOMDocument 
//object.  We will create an ns 
// 
Document::Document() 
{ 
  /* XXX (Pvdb)
     Do we really need this? It causes a link dependency on layout, so i
     commented it out 'till we figure out what to do.

  nsCOMPtr<nsIDocument> document;
  nsresult res = NS_NewXMLDocument(getter_AddRefs(document));
  if (NS_SUCCEEDED(res) && document) {
    document->QueryInterface(NS_GET_IID(nsIDOMDocument), (void**) &nsDocument);
  }
  Node::setNSObj(nsDocument, this);*/
}
 
// 
//Construct a Document Wrapper object with a nsIDOMDocument object 
// 
Document::Document(nsIDOMDocument* document) : Node(document, this) 
{ 
  nsDocument = document; 
} 
 
// 
//Clean up the private data members 
// 
Document::~Document() 
{ 
} 
 
// 
//Provide an nsIDOMDocument object for wrapping 
// 
void Document::setNSObj(nsIDOMDocument* document) 
{ 
  Node::setNSObj(document); 
  nsDocument = document; 
} 
 
// 
//Call nsIDOMDocument::GetDocumentElement to retreive the element for this 
//document.  Then call the createElement factory method to retreive an element 
//wrapper for this object. 
// 
Element* Document::getDocumentElement() 
{ 
  nsIDOMElement* theElement = NULL; 
  Element* elemWrapper = NULL; 
  nsresult retval = 5; 
 
  if (nsDocument == NULL) 
    return NULL; 
 
  //retval = nsDocument->GetDocumentElement(&theElement); 
  //if (retval == NS_OK) 
  if ((retval = nsDocument->GetDocumentElement(&theElement)) == NS_OK) 
    { 
      cout << "Document::getDocumentElement - Mozilla Call ok" << endl; 
      return createElement(theElement); 
    } 
  else 
    { 
      cout << "Document::getDocumentElement - Mozilla Call not ok(" << hex <<retval << ")" << endl; 
      return NULL; 
    } 
} 
 
// 
//Retrieve the DocumentType from nsIDOMDocument::GetDocType.  Return it wrapped 
//in the docType data member. 
// 
DocumentType* Document::getDoctype() 
{ 
  nsIDOMDocumentType* theDocType = NULL; 
 
  if (nsDocument == NULL) 
    return NULL; 
 
  if (nsDocument->GetDoctype(&theDocType) == NS_OK) 
    return createDocumentType(theDocType); 
  else 
    return NULL; 
} 
 
// 
//Return a constant reference to the DOM's Implementation 
// 
DOMImplementation* Document::getImplementation() 
{ 
  nsIDOMDOMImplementation* theImpl = NULL; 
 
  if (nsDocument == NULL) 
    return NULL; 
 
  if (nsDocument->GetImplementation(&theImpl) == NS_OK) 
    return createDOMImplementation(theImpl); 
  else 
    return NULL; 
} 
 
// 
//Ensure that no Element node is inserted if the document already has an 
//associated Element child. 
// 
Node* Document::insertBefore(Node* newChild, Node* refChild) 
{ 
  /* XXX HACK (Pvdb)
     Work around Bugzilla bug #25123, we can't do insertBefore for the
     first node. So we fiddle with SetRootContent. If the bug gets
     resolved, defer to node implementation.
  */
  nsIDOMNode* returnValue = NULL;

  if (nsDocument == NULL)
    return NULL;

  nsCOMPtr<nsIDocument> nsTempDocument = do_QueryInterface(nsDocument);

  if (nsTempDocument->GetRootContent() && (newChild->getNodeType() != Node::ELEMENT_NODE)) {
    if (nsDocument->InsertBefore(newChild->getNSObj(), refChild->getNSObj(),
			     &returnValue) == NS_OK)
      return ownerDocument->createWrapper(returnValue);
    else
      return NULL;
  }
  else
  {
    nsCOMPtr<nsIContent> nsRootContent = do_QueryInterface(newChild->getNSObj());
    nsIDOMElement* theElement = NULL; 

    nsTempDocument->SetRootContent(nsRootContent);
    nsDocument->GetDocumentElement(&theElement); 
    return ownerDocument->createWrapper(theElement);
  }
} 
 
// 
//  DEFFER this functionality to the NODE implementation 
//Ensure that if the newChild is an Element and the Document already has an 
//element, then oldChild should be specifying the existing element.  If not 
//then the replacement can not take place. 
// 
/* 
Node* Document::replaceChild(Node* newChild, Node* oldChild) 
{ 
  Node* replacedChild = NULL; 
 
  if (newChild->getNodeType() != Node::ELEMENT_NODE) 
    { 
      //The new child is not an Element, so perform replacement 
      replacedChild = NodeDefinition::replaceChild(newChild, oldChild); 
 
      //If old node was an Element, then the document's element has been 
      //replaced with a non-element node.  Therefore clear the documentElement 
      //pointer 
      if (replacedChild && (oldChild->getNodeType() == Node::ELEMENT_NODE)) 
        documentElement = NULL; 
 
      return replacedChild; 
    } 
  else 
    { 
      //A node is being replaced with an Element.  If the document does not 
      //have an elemet yet, then just allow the replacemetn to take place. 
      if (!documentElement) 
        replacedChild = NodeDefinition::replaceChild(newChild, oldChild); 
      else if (oldChild->getNodeType() == Node::ELEMENT_NODE) 
        replacedChild = NodeDefinition::replaceChild(newChild, oldChild); 
 
      if (replacedChild) 
        documentElement = (Element*)newChild; 
 
      return replacedChild; 
    } 
} 
*/ 
 
// 
//  DEFFER this functionality to the NODE Implementation 
//Update the documentElement pointer if the associated Element node is being 
//removed. 
// 
/* 
Node* Document::removeChild(Node* oldChild) 
{ 
  Node* removedChild = NULL; 
 
  removedChild = NodeDefinition::removeChild(oldChild); 
 
  if (removedChild && (removedChild->getNodeType() == Node::ELEMENT_NODE)) 
    documentElement = NULL; 
 
  return removedChild; 
} 
*/ 
 
Node* Document::appendChild(Node* newChild)
{
  /* XXX HACK (Pvdb)
     Work around Bugzilla bug #25123, we can't do appendChild for the
     first node. So we fiddle with SetRootContent. If the bug gets
     resolved, defer to node implementation.
  */
  nsIDOMNode* returnValue = NULL;

  if (nsDocument == NULL)
    return NULL;

  nsCOMPtr<nsIDocument> nsTempDocument = do_QueryInterface(nsDocument);

  if (nsTempDocument->GetRootContent() && (newChild->getNodeType() != Node::ELEMENT_NODE)) {
    if (nsDocument->AppendChild(newChild->getNSObj(), &returnValue) == NS_OK)
      return ownerDocument->createWrapper(returnValue);
    else
      return NULL;
  }
  else
  {
    nsCOMPtr<nsIContent> nsRootContent = do_QueryInterface(newChild->getNSObj());
    nsIDOMElement* theElement = NULL; 

    nsTempDocument->SetRootContent(nsRootContent);
    nsDocument->GetDocumentElement(&theElement); 
    return ownerDocument->createWrapper(theElement);
  }
}

// 
//Call the nsIDOMDocument::CreateDocumentFragment function, then create or 
//retrieve a wrapper class for it. 
// 
DocumentFragment* Document::createDocumentFragment() 
{ 
  nsIDOMDocumentFragment* fragment = NULL; 
 
  if (nsDocument == NULL) 
    return NULL; 
 
  if (nsDocument->CreateDocumentFragment(&fragment) == NS_OK) 
    return createDocumentFragment(fragment); 
  else 
    return NULL; 
} 
 
// 
//Check the wrapperHashTable for a preexisting wrapper object.  If it does not 
// exists, create one and store it in the hash table.  If it does exist, simply 
// return it to the caller. 
// 
DocumentFragment*  
   Document::createDocumentFragment(nsIDOMDocumentFragment* fragment) 
{ 
  DocumentFragment* docFragWrapper = NULL; 
 
  if (fragment) 
    { 
      docFragWrapper =  
	(DocumentFragment*)wrapperHashTable.retrieve((Int32)fragment); 
       
      if (!docFragWrapper) 
	{ 
	  docFragWrapper = new DocumentFragment(fragment, this); 
	  wrapperHashTable.add(docFragWrapper, (Int32)fragment); 
	} 
    } 
 
  return docFragWrapper; 
} 
 
// 
//Call the nsIDOMDocument::CreateElement function, and retrieve or create 
//a wrapper object for it. 
// 
Element* Document::createElement(const DOMString& tagName) 
{ 
  nsIDOMElement* element = NULL; 
 
  if (nsDocument->CreateElement(tagName.getConstNSString(), &element) == NS_OK) 
    return createElement(element); 
  else 
    return NULL; 
} 
 
// 
//Check the wrapperHashTable for a precreated wrapper object.  If one exists,  
//return it to the caller.  If it doens't exist, create one, store it in the  
//hash, and return it to the caller. 
// 
Element* Document::createElement(nsIDOMElement* element) 
{ 
  Element* elemWrapper = NULL; 
 
  if (element) 
    { 
      elemWrapper = (Element*)wrapperHashTable.retrieve((Int32)element); 
       
      if (!elemWrapper) 
	{ 
	  elemWrapper = new Element(element, this); 
	  wrapperHashTable.add(elemWrapper, (Int32)element); 
	} 
    } 
 
  return elemWrapper; 
} 
 
// 
//Call the nsIDOMDocument::CreateAttribute function, then create or retrieve a 
//wrapper object for it. 
// 
Attr* Document::createAttribute(const DOMString& name) 
{ 
  nsIDOMAttr* attr = NULL; 
 
  if (nsDocument == NULL) 
    return NULL; 
 
  if (nsDocument->CreateAttribute(name.getConstNSString(), &attr) == NS_OK) 
    return createAttribute(attr); 
  else 
    return NULL; 
} 
 
// 
//Check the wrapperHashTable for a precreated object.  If one exists, return it, 
//else, create one, store it in the table, and return it. 
// 
Attr* Document::createAttribute(nsIDOMAttr* attr) 
{ 
  Attr* attrWrapper = NULL; 
 
  if (attr) 
    { 
      attrWrapper = (Attr*)wrapperHashTable.retrieve((Int32)attr); 
 
      if (!attrWrapper) 
	{ 
	  attrWrapper = new Attr(attr, this); 
	  wrapperHashTable.add(attrWrapper, (Int32)attr); 
	} 
    } 
 
  return attrWrapper; 
} 
 
// 
//Call the nsIDOMDocument::CreateTextNode function, then create, or retrieve a  
//wrapper object for it. 
// 
Text* Document::createTextNode(const DOMString& theData) 
{ 
  nsIDOMText* text = NULL; 
 
  if (nsDocument == NULL) 
    return NULL; 
 
  if (nsDocument->CreateTextNode(theData.getConstNSString(), &text) == NS_OK) 
    return createTextNode(text); 
  else 
    return NULL; 
} 
 
// 
//Check the wrapperHashTable to see if a precreated object exists.  If it does  
//then return it.  Else, create a new one, insert it into the hash table, and  
//then return it. 
// 
Text* Document::createTextNode(nsIDOMText* text) 
{ 
  Text* textWrapper = NULL; 
 
  if (text) 
    { 
      textWrapper = (Text*)wrapperHashTable.retrieve((Int32)text); 
 
      if (!textWrapper) 
	{ 
	  textWrapper = new Text(text, this); 
	  wrapperHashTable.add(textWrapper, (Int32)text); 
	} 
    } 
 
  return textWrapper; 
} 
 
// 
//Call the nsIDOMDocument::CreateComment function, then create or retrieve a 
//wrapper object to return to the caller. 
// 
Comment* Document::createComment(const DOMString& theData) 
{ 
  nsIDOMComment* comment = NULL; 
 
  if (nsDocument == NULL) 
    return NULL; 
 
  if (nsDocument->CreateComment(theData.getConstNSString(), &comment) == NS_OK) 
    return createComment(comment); 
  else 
    return NULL; 
} 
 
// 
//Check the wrapperHashTable for a precreated object to return to the caller. 
//If one does not exist, create it, store it in the hash, and return it to the 
//caller. 
Comment* Document::createComment(nsIDOMComment* comment) 
{ 
  Comment* commentWrapper = NULL; 
 
  if (comment) 
    { 
      commentWrapper = (Comment*)wrapperHashTable.retrieve((Int32)comment); 
 
      if (!commentWrapper) 
	{ 
	  commentWrapper = new Comment(comment, this); 
	  wrapperHashTable.add(commentWrapper, (Int32)comment); 
	} 
    } 
 
  return commentWrapper; 
} 
 
// 
//Call the nsIDOMDocument::CreateCDATASection function, then create or retrieve 
//a wrapper object to return to the caller. 
// 
CDATASection* Document::createCDATASection(const DOMString& theData) 
{ 
  nsIDOMCDATASection* cdata = NULL; 
 
  if (nsDocument == NULL) 
    return NULL; 
 
  if (nsDocument->CreateCDATASection(theData.getConstNSString(), &cdata) ==  
      NS_OK) 
    return createCDATASection(cdata); 
  else 
    return NULL; 
} 
 
// 
//Check to see if a precreated object exists in the wrapperHashTable.  If so 
//simply return it to the caller.  If not, creat one, store it in the hash, and 
//return it to the caller. 
// 
CDATASection* Document::createCDATASection(nsIDOMCDATASection* cdata) 
{ 
  CDATASection* cdataWrapper = NULL; 
 
  if (cdata) 
    { 
      cdataWrapper = (CDATASection*)wrapperHashTable.retrieve((Int32)cdata); 
 
      if (!cdataWrapper) 
	{ 
	  cdataWrapper = new CDATASection(cdata, this); 
	  wrapperHashTable.add(cdataWrapper, (Int32)cdata); 
	} 
    } 
 
  return cdataWrapper; 
} 
 
// 
//Call the nsIDOMDocument::CreateProcessingInstruction function, then find or 
//create a wrapper class to return to the caller. 
// 
ProcessingInstruction* 
  Document::createProcessingInstruction(const DOMString& target, 
                                        const DOMString& data) 
{ 
  nsIDOMProcessingInstruction* pi = NULL; 
 
  if (nsDocument == NULL) 
    return NULL; 
 
  if (nsDocument->CreateProcessingInstruction(target.getConstNSString(), 
				       data.getConstNSString(), &pi) == NS_OK) 
    return createProcessingInstruction(pi); 
  else 
    return NULL; 
} 
 
// 
//Check the wrapperHashTable for a precreated object.  If one exists, return 
//it to the caller.  If not, create one, store it in the hash table, and then 
//return it to the caller. 
// 
ProcessingInstruction*  
Document::createProcessingInstruction(nsIDOMProcessingInstruction* pi) 
{ 
  ProcessingInstruction* piWrapper = NULL; 
 
  if (pi) 
    { 
      piWrapper = (ProcessingInstruction*)wrapperHashTable.retrieve((Int32)pi); 
 
      if (!piWrapper) 
	{ 
	  piWrapper = new ProcessingInstruction(pi, this); 
	  wrapperHashTable.add(piWrapper, (Int32)pi); 
	} 
    } 
 
  return piWrapper; 
} 
 
// 
//Call the nsIDOMDocument::CreateEntityReference function, then obtain a wrapper 
//class to return to the user. 
// 
EntityReference* Document::createEntityReference(const DOMString& name) 
{ 
  nsIDOMEntityReference* entityRef = NULL; 
 
  if (nsDocument == NULL) 
    return NULL; 
 
  if (nsDocument->CreateEntityReference(name.getConstNSString(),  
					&entityRef) == NS_OK) 
    return createEntityReference(entityRef); 
  else 
    return NULL; 
} 
 
// 
//Check the wrapperHashTable for a wrapper class.  If one exists, simply return 
//it to the caller.  Else, create one, place it in the hash, and return it to  
//the caller. 
// 
EntityReference*  
Document::createEntityReference(nsIDOMEntityReference* entityRef) 
{ 
  EntityReference* entityWrapper = NULL; 
 
  if (entityRef) 
    { 
      entityWrapper =  
	(EntityReference*) wrapperHashTable.retrieve((Int32)entityRef); 
 
      if (!entityWrapper) 
	{ 
	  entityWrapper = new EntityReference(entityRef, this); 
	  wrapperHashTable.add(entityWrapper, (Int32)entityRef); 
	} 
    } 
 
  return entityWrapper; 
} 
 
// 
//Check the wrapperHashTable for a wrapper class.  If one exists, simply return 
//it to the caller.  Else create one, place it in the hash table, and then return 
//it to the caller. 
Entity* Document::createEntity(nsIDOMEntity* entity) 
{ 
  Entity* entityWrapper = NULL; 
 
  if (entity) 
    { 
      entityWrapper = (Entity*)wrapperHashTable.retrieve((Int32) entity); 
 
      if (!entity) 
	{ 
	  entityWrapper = new Entity(entity, this); 
	  wrapperHashTable.add(entityWrapper, (Int32)entity); 
	} 
    } 
 
  return entityWrapper; 
} 
 
// 
//Factory function for creating and hashing generic Node wrapper classes. 
//Check the wrapperHashTable to see if a wrapper object already exists.  If it 
//does then return it.  If one does not exist, then creat one, hash it, and  
//return it to the caller. 
// 
Node* Document::createNode(nsIDOMNode* node) 
{ 
  Node* nodeWrapper = NULL; 
 
  if (node) 
    { 
      nodeWrapper = (Node*)wrapperHashTable.retrieve((Int32)node); 
 
      if (!nodeWrapper) 
	{ 
	  nodeWrapper = new Node(node, this); 
	  wrapperHashTable.add(nodeWrapper, (Int32)node); 
	} 
    } 
 
  return nodeWrapper; 
} 
 
// 
//Factory function for creating and hashing a DOMString object.  Note that it 
//is specifically for wrapping nsString objects with MozillaString objects. 
//Once the object is created it is stored in the hash table. 
// 
DOMString* Document::createDOMString(nsString* str) 
{ 
  DOMString* strWrapper = NULL; 
 
  if (str) 
    { 
      strWrapper = (DOMString*)wrapperHashTable.retrieve((Int32)str); 
 
      if (!strWrapper) 
	{ 
	  strWrapper = new DOMString(str); 
	  wrapperHashTable.add(strWrapper, (Int32)str); 
	} 
    } 
 
  return strWrapper; 
} 
 
// 
//Factory function for creating and hashing a Notation object. 
// 
Notation* Document::createNotation(nsIDOMNotation* notation) 
{ 
  Notation* notationWrapper = NULL; 
 
  if (notation) 
    { 
      notationWrapper = (Notation*)wrapperHashTable.retrieve((Int32) notation); 
 
      if (!notationWrapper) 
	{ 
	  notationWrapper = new Notation(notation, this); 
	  wrapperHashTable.add(notationWrapper, (Int32)notation); 
	} 
    } 
 
  return notationWrapper; 
} 
 
// 
//Factory function for creating and hashing DOM Implementation objects 
// 
DOMImplementation*  
Document::createDOMImplementation(nsIDOMDOMImplementation* impl) 
{ 
  DOMImplementation* implWrapper = NULL; 
 
  if (impl) 
    { 
      implWrapper = (DOMImplementation*)wrapperHashTable.retrieve((Int32)impl); 
 
      if (!implWrapper) 
	{ 
	  implWrapper = new DOMImplementation(impl, this); 
	  wrapperHashTable.add(implWrapper, (Int32)impl); 
	} 
    } 
 
  return implWrapper; 
} 
 
// 
//Factory function for creating and hashing DOM DocumentType objects. 
// 
DocumentType* Document::createDocumentType(nsIDOMDocumentType* doctype) 
{ 
  DocumentType* doctypeWrapper = NULL; 
 
  if (doctype) 
    { 
      doctypeWrapper = (DocumentType*)wrapperHashTable.retrieve((Int32)doctype); 
 
      if (!doctypeWrapper) 
	{ 
	  doctypeWrapper = new DocumentType(doctype, this); 
	  wrapperHashTable.add(doctypeWrapper, (Int32)doctype); 
	} 
    } 
 
  return doctypeWrapper; 
} 
 
// 
//Factory function for creating and hashing NodeList objects 
// 
NodeList* Document::createNodeList(nsIDOMNodeList* list) 
{ 
  NodeList* listWrapper = NULL; 
 
  if (list) 
    { 
      listWrapper = (NodeList*)wrapperHashTable.retrieve((Int32)list); 
 
      if (!listWrapper) 
	{ 
	  listWrapper = new NodeList(list, this); 
	  wrapperHashTable.add(listWrapper, (Int32)list); 
	} 
    } 
 
  return listWrapper; 
} 
 
// 
//Factory function for creating and hashing NamedNodeMap objects 
// 
NamedNodeMap* Document::createNamedNodeMap(nsIDOMNamedNodeMap* map) 
{ 
  NamedNodeMap* mapWrapper = NULL; 
 
  if (map) 
    { 
      mapWrapper = (NamedNodeMap*)wrapperHashTable.retrieve((Int32)map); 
 
      if (!mapWrapper) 
	{ 
	  mapWrapper = new NamedNodeMap(map, this); 
	  wrapperHashTable.add(mapWrapper, (Int32)map); 
	} 
    } 
 
  return mapWrapper; 
} 
 
// 
//Remove the specified hashed object from wrapperHashTable and return it to the 
//caller. 
// 
MITREObject* Document::removeWrapper(Int32 hashValue) 
{ 
  return wrapperHashTable.remove(hashValue); 
} 
 
// 
//Add the specified wrapper to the hash table using the specified hash value 
// 
void Document::addWrapper(MITREObject* obj, Int32 hashValue) 
{ 
  wrapperHashTable.add(obj, hashValue); 
} 
 
// 
//Determine what kind of node this is, and create the appropriate wrapper for it 
// 
Node* Document::createWrapper(nsIDOMNode* node) 
{ 
  unsigned short nodeType = 0; 
 
  // 
  //TK 02/15/2000 - Must make sure node is not null. 
  if (!node) 
    return NULL; 
 
  node->GetNodeType(&nodeType); 
 
  switch (nodeType) 
    { 
    case nsIDOMNode::ELEMENT_NODE: 
      return createElement((nsIDOMElement*)node); 
      break; 
 
    case nsIDOMNode::ATTRIBUTE_NODE: 
      return createAttribute((nsIDOMAttr*)node); 
      break; 
 
    case nsIDOMNode::TEXT_NODE: 
      return createTextNode((nsIDOMText*)node); 
      break; 
 
    case nsIDOMNode::CDATA_SECTION_NODE: 
      return createCDATASection((nsIDOMCDATASection*)node); 
      break; 
 
    case nsIDOMNode::COMMENT_NODE: 
      return createComment((nsIDOMComment*)node); 
      break; 
 
    case nsIDOMNode::ENTITY_REFERENCE_NODE: 
      return createEntityReference((nsIDOMEntityReference*)node); 
      break; 
 
    case nsIDOMNode::ENTITY_NODE: 
      return createEntity((nsIDOMEntity*)node); 
      break; 
 
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE: 
      return createProcessingInstruction((nsIDOMProcessingInstruction*)node); 
      break; 
 
    case nsIDOMNode::DOCUMENT_TYPE_NODE: 
      return createDocumentType((nsIDOMDocumentType*)node); 
      break; 
 
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE: 
      return createDocumentFragment((nsIDOMDocumentFragment*)node); 
      break; 
       
    case nsIDOMNode::NOTATION_NODE: 
      return createNotation((nsIDOMNotation*)node); 
      break; 
 
    default: 
      return createNode(node); 
    } 
} 
