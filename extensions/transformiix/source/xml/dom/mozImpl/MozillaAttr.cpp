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
//  Wrapper class to convert Mozilla's Attr Interface to TransforMIIX's Attr 
//  Interface. 
// 
//  NOTE:  The objects being wrapped are not deleted.  It is assumed that they 
//         will be deleted when the actual ("real") Mozilla Document is 
//         destroyed 
// 
// Modification History: 
// Who  When        What 
// 
 
#include "mozilladom.h" 
#include "nsCOMPtr.h"
#include "nsIXMLContent.h"
 
// 
//Construct an Attribute wrapper object using the specified nsIDOMAttr object 
// 
Attr::Attr(nsIDOMAttr* attr, Document* owner) : Node(attr, owner) 
{ 
  nsAttr = attr; 
} 
 
// 
//Destroy the delegate nsIDOMAttr object 
// 
Attr::~Attr() 
{ 
} 
 
// 
//Return the nsAttr data member to the caller. 
// 
nsIDOMAttr* Attr::getNSAttr() 
{ 
  return nsAttr; 
} 
 
void Attr::setNSObj(nsIDOMAttr* attr) 
{ 
  Node::setNSObj(attr); 
  nsAttr = attr; 
} 
 
// 
//Retrieve the name of the attribute storing it in a String wrapper 
//class.  The wrapper is retrieved from the Document so it can be hashed.  At 
//this time no one else will use this hased object since it has been  
//specifically created for the caller of this function, but at lease its  
//memory managment is handled automatically.  In the future, a more efficient 
//implementation can be created based on the characters stored in the string 
//or something.   
//   NOTE:  We don't need to worry about memory management here since the 
//          String object will delete its nsString object automatically 
// ( nsIDOMAttr::GetName(nsString&) ) 
// 
const String& Attr::getName() 
{ 
  nsString* name = new nsString(); 
  nsString prefix; 
 
  /* XXX HACK (pvdb)
     This can be removed once we have DOM Level 2 support
     in Mozilla
  */
  if (nsAttr->GetName(*name) == NS_OK)
    {
      /* XXX HACK (pvdb)
         This can be removed once we have DOM Level 2 support
         in Mozilla
      */
      if (NS_SUCCEEDED(nsAttr->GetPrefix(prefix)) && (prefix.Length() > 0))
      {
          name->InsertWithConversion(":", 0);
          name->Insert(prefix, 0);
      }
      return *(ownerDocument->createDOMString(name));
    }
  else 
    { 
      //name won't be used, so delete it. 
      delete name; 
      return NULL_STRING; 
    } 
} 
 
// 
//Retrieve the specified flag.  ( nsIDOMAttr::GetSpecified(PRBool*) ) 
// 
MBool Attr::getSpecified() const 
{ 
  MBool specified; 
 
  nsAttr->GetSpecified(&specified); 
 
  return specified; 
} 
 
// 
//Retrieve the value of the attribute. See getName above for hashing info 
// 
const String& Attr::getValue() 
{ 
  nsString* value = new nsString(); 
 
  if (nsAttr->GetValue(*value) == NS_OK) 
    return *(ownerDocument->createDOMString(value)); 
  else 
    { 
      //value is not needed so delete it. 
      delete value; 
      return NULL_STRING; 
    } 
} 
 
// 
//Foward call to nsIDOMAttr::SetValue. 
// 
void Attr::setValue(const String& newValue) 
{ 
  nsAttr->SetValue(newValue.getConstNSString()); 
} 
 
 
// 
//Override the set node value member function to create a new TEXT node with 
//the String and to add it as the Attribute's child. 
//    NOTE:  Not currently impemented, just execute the default setNodeValue 
// 
//void Attr::setNodeValue(const String& nodeValue) 
//{ 
//  setValue(nodeValue); 
//} 
 
// 
//Return a String represening the value of this node.  If the value is an 
//Entity Reference then return the value of the reference.  Otherwise, it is a 
//simple conversion of the text value. 
//    NOTE: Not currently implemented, just execute the default getNodeValue 
// 
//const String& Attr::getNodeValue() 
//{ 
//  return getValue(); 
//} 
 
 
// 
//First check to see if the new node is an allowable child for an Attr.  If it 
//is, call NodeDefinition's implementation of Insert Before.  If not, return 
//null as an error. 
// 
//Node* Attr::insertBefore(Node* newChild, Node* refChild) 
//{ 
//  Node* returnVal = NULL; 
// 
//  switch (newChild->getNodeType()) 
//    { 
//      case Node::TEXT_NODE : 
//      case Node::ENTITY_REFERENCE_NODE: 
//        returnVal = NodeDefinition::insertBefore(newChild, refChild); 
// 
//        if (returnVal) 
//          specified = MB_TRUE; 
//        break; 
//      default: 
//        returnVal = NULL; 
//    } 
// 
//  return returnVal; 
//}
