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
//    Implementation of the Element class
//
// Modification History:
// Who  When      What
// TK   03/29/99  Created
// LF   08/06/1999  fixed typo: defalut to default
//

#include "dom.h"

//
//Construct a new element with the specified tagName and Document owner.
//Simply call the constructor for NodeDefinition, and specify the proper node
//type.
//
Element::Element(const String& tagName, Document* owner) :
         NodeDefinition(Node::ELEMENT_NODE, tagName, NULL_STRING, owner)
{
}

//
//First check to see if the new node is an allowable child for an Element.  If
//it is, call NodeDefinition's implementation of Insert Before.  If not, return
//null as an error
//
Node* Element::insertBefore(Node* newChild, Node* refChild)
{
  Node* returnVal = NULL;

  switch (newChild->getNodeType())
    {
      case Node::ELEMENT_NODE :
      case Node::TEXT_NODE :
      case Node::COMMENT_NODE :
      case Node::PROCESSING_INSTRUCTION_NODE :
      case Node::CDATA_SECTION_NODE :
      case Node::DOCUMENT_FRAGMENT_NODE : //-- added 19990813 (kvisco)
      case Node::ENTITY_REFERENCE_NODE:
        returnVal = NodeDefinition::insertBefore(newChild, refChild);
        break;
      default:
        returnVal = NULL;
    }

  return returnVal;
}

//
//Return the tagName for this element.  This is simply the nodeName.
//
const String& Element::getTagName()
{
  return nodeName;
}

//
//Retreive an attribute's value by name.  If the attribute does not exist,
//return a reference to the pre-created, constatnt "NULL STRING".
//
const String& Element::getAttribute(const String& name)
{
  Node* tempNode = attributes.getNamedItem(name);

  if (tempNode)
    return attributes.getNamedItem(name)->getNodeValue();
  else
    return NULL_STRING;
}


//
//Add an attribute to this Element.  Create a new Attr object using the
//name and value specified.  Then add the Attr to the the Element's
//attributes NamedNodeMap.
//
void Element::setAttribute(const String& name, const String& value)
{
  Attr* tempAttribute;

  //Check to see if an attribute with this name already exists.  If it does
  //over write its value, if not, add it.
  tempAttribute = getAttributeNode(name);
  if (tempAttribute)
      tempAttribute->setNodeValue(value);
  else
    {
      tempAttribute = getOwnerDocument()->createAttribute(name);
      tempAttribute->setNodeValue(value);
      attributes.setNamedItem(tempAttribute);
    }
}

//
//Remove an attribute from the attributes NamedNodeMap, and free its memory.
//   NOTE:  How do default values enter into this picture
//
void Element::removeAttribute(const String& name)
{
  delete attributes.removeNamedItem(name);
}

//
//Return the attribute specified by name
//
Attr* Element::getAttributeNode(const String& name)
{
  return (Attr*)attributes.getNamedItem(name);
}

//
//Set a new attribute specifed by the newAttr node.  If an attribute with that
//name already exists, the existing Attr is removed from the list and return to
//the caller, else NULL is returned.
//
Attr* Element::setAttributeNode(Attr* newAttr)
{
  Attr* pOldAttr = (Attr*)attributes.removeNamedItem(newAttr->getNodeName());

  attributes.setNamedItem(newAttr);
  return pOldAttr;
}

//
//Remove the Attribute from the attributes list and return to the caller.  If
//the node is not found, return NULL.
//
Attr* Element::removeAttributeNode(Attr* oldAttr)
{
  return (Attr*)attributes.removeNamedItem(oldAttr->getNodeName());
}

NodeList* Element::getElementsByTagName(const String& name)
{
    return 0;
}

void Element::normalize()
{
}
