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
//  Implementation of the Element wrapper class 
// 
//  NOTE:  Return values that are references to DOMStrings are all hashed 
//         and maintained in the owning document's wrapper hash table. 
//         They will not necessarly be used by more than one caller, but 
//         at least their memory management is taken care of.  This can be done 
//         because all MozillaStrings clean up their nsString object upon 
//         deletion. 
// 
// Modification History: 
// Who  When      What 
// 
 
#include "mozilladom.h" 
#include "iostream.h" 
 
 
// 
//Construct a new element wrapper simply storing the nsIDOMElement object 
// 
Element::Element(nsIDOMElement* element, Document* owner) : Node(element, owner) 
{ 
  nsElement = element; 
} 
 
// 
//Default destructor for Element wrappers 
// 
Element::~Element() 
{ 
} 
 
// 
//Store a new subject object for this wrapper 
// 
void Element::setNSObj(nsIDOMElement* element) 
{ 
  Node::setNSObj(element); 
  nsElement = element; 
} 
 
// 
//First check to see if the new node is an allowable child for an Element.  If 
//it is, call NodeDefinition's implementation of Insert Before.  If not, return 
//null as an error 
// 
/*Node* Element::insertBefore(Node* newChild, Node* refChild) 
{ 
  Node* returnVal = NULL; 
 
  switch (newChild->getNodeType()) 
    { 
      case Node::ELEMENT_NODE : 
      case Node::TEXT_NODE : 
      case Node::COMMENT_NODE : 
      case Node::PROCESSING_INSTRUCTION_NODE : 
      case Node::CDATA_SECTION_NODE : 
      case Node::ENTITY_REFERENCE_NODE: 
        returnVal = NodeDefinition::insertBefore(newChild, refChild); 
        break; 
      default: 
        returnVal = NULL; 
    } 
 
  return returnVal; 
}*/ 
 
// 
//Call nsIDOMElement::GetTagName to retrieve the tag name for this element. 
//If the call complests successfully get a new DOMString wrapper from the 
//owner document. 
// 
const DOMString& Element::getTagName() 
{ 
  nsString* tagName = new nsString(); 
 
  if (nsElement->GetTagName(*tagName) == NS_OK) 
    return *(ownerDocument->createDOMString(tagName)); 
  else 
    { 
      //tagName is not needed so destroy it. 
      delete tagName; 
      return NULL_STRING; 
    } 
} 
 
// 
//Call nsIDOMElement::GetAttribute to retrieve value for the specified  
//attribute.  Defer to the owner document to request a new DOMString 
//wrapper. 
const DOMString& Element::getAttribute(const DOMString& name) 
{ 
  nsString* attrValue = new nsString(); 
   
  if (nsElement->GetAttribute(name.getConstNSString(), *attrValue) == NS_OK) 
    return *(ownerDocument->createDOMString(attrValue)); 
  else 
    { 
      delete attrValue; 
      return NULL_STRING; 
    } 
} 
 
 
// 
//Call nsIDOMElement::SetAttribute to create a new attribute. 
// 
void Element::setAttribute(const DOMString& name, const DOMString& value) 
{ 
  nsElement->SetAttribute(name.getConstNSString(), value.getConstNSString()); 
} 
 
// 
//We need to make this call a bit more complicated than usual because we want 
//to make sure we remove the attribute wrapper from the document  
//wrapperHashTable. 
// 
void Element::removeAttribute(const DOMString& name) 
{ 
  nsIDOMAttr* attr = NULL; 
  Attr*  attrWrapper = NULL; 
 
  //Frist, get the nsIDOMAttr object from the nsIDOMElement object 
  nsElement->GetAttributeNode(name.getConstNSString(), &attr); 
 
  //Second, remove the attribute wrapper object from the hash table if it is 
  //there.  It might not be if the attribute was created using  
  //Element::setAttribute.  If it was removed, then delete it. 
  attrWrapper = (Attr*)ownerDocument->removeWrapper((Int32)attr); 
  if (attrWrapper) 
    delete attrWrapper; 
 
  //Lastly, have the Mozilla ojbect remove the attribute 
  nsElement->RemoveAttribute(name.getConstNSString()); 
} 
 
// 
//Call nsIDOMElement::GetAttributeNode.  If successful, refer to the owner  
//document for an attribute wrapper class. 
// 
Attr* Element::getAttributeNode(const DOMString& name) 
{ 
  nsIDOMAttr* attr = NULL; 
 
  if (nsElement->GetAttributeNode(name.getConstNSString(), &attr) == NS_OK) 
    return ownerDocument->createAttribute(attr); 
  else 
    return NULL; 
} 
 
// 
//Call nsIDOMElement::SetAttributeNode passing it the nsIDOMAttr object wrapped 
//by the newAttr parameter. 
// 
Attr* Element::setAttributeNode(Attr* newAttr) 
{ 
  nsIDOMAttr* returnAttr = NULL; 
 
  if (nsElement->SetAttributeNode(newAttr->getNSAttr(), &returnAttr) == NS_OK) 
    return ownerDocument->createAttribute(returnAttr); 
  else 
    return NULL; 
} 
 
// 
//Call nsIDOMElement::RemoveAttributeNode, then refer to the owner document to 
//remove it from the hash.  The caller is then responsible for destroying the 
//wrappr. 
//          NOTE:  Do we need to worry about the memory used by the wrapped 
//                 Mozilla object? 
// 
Attr* Element::removeAttributeNode(Attr* oldAttr) 
{ 
  nsIDOMAttr* removedAttr = NULL; 
  Attr* attrWrapper = NULL; 
 
  if (nsElement->RemoveAttributeNode(oldAttr->getNSAttr(), &removedAttr) == NS_OK) 
    { 
      attrWrapper = (Attr*)ownerDocument->removeWrapper((Int32)removedAttr); 
      if (!attrWrapper) 
	attrWrapper =  new Attr(removedAttr, ownerDocument); 
 
	return attrWrapper; 
    } 
  else 
    return NULL; 
} 
 
// 
//Call nsIDOMElement::GetElementsByTagName.  If successful, defer to the owning 
//documet to produce a wrapper for this object. 
NodeList* Element::getElementsByTagName(const DOMString& name) 
{ 
  nsIDOMNodeList* list = NULL; 
 
  if (nsElement->GetElementsByTagName(name.getConstNSString(), &list) == NS_OK) 
    return ownerDocument->createNodeList(list); 
  else 
    return NULL; 
} 
 
// 
//Simply call nsIDOMElement::Normalize() 
// 
void Element::normalize() 
{ 
  nsElement->Normalize();   
} 
