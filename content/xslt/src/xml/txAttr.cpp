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
//    Implementation of the Attr class
//
// Modification History:
// Who  When        What
// TK   03/29/99    Created
// LF   08/06/1999  fixed typo: defalut to default

//

#include "dom.h"

//
//Construct an Attribute object using the specified name and document owner
//
Attr::Attr(const String& name, Document* owner):
      NodeDefinition(Node::ATTRIBUTE_NODE, name, NULL_STRING, owner)
{
  specified = MB_FALSE;
}

//
//Retrieve the name of the attribute from the nodeName data member
//
const String& Attr::getName() const
{
  return nodeName;
}

//
//Retrieve the specified flag
//
MBool Attr::getSpecified() const
{
  return specified;
}

//
//Retrieve the value of the attribute.  This is a comma-deliminated string
//representation of the Attribute's children.
//
const String& Attr::getValue()
{
  Int32 valueLoop;
  nodeValue = NULL_STRING;
  NodeList* childList = getChildNodes();
  Int32 numChildren = childList->getLength();

  for (valueLoop=0;valueLoop<numChildren;valueLoop++)
    {
    if (childList->item(valueLoop)->getNodeType() != Node::ENTITY_REFERENCE_NODE)
      {
        nodeValue.append(childList->item(valueLoop)->getNodeValue());
        if (valueLoop < (numChildren-1))
          nodeValue.append(",");
      }
    }

  return nodeValue;
}

//
//Create a new Text node and add it to the Attribute's list of children.  Also
//set the Specified flag to true.
//
void Attr::setValue(const String& newValue)
{
  NodeDefinition::DeleteChildren();

  appendChild(getOwnerDocument()->createTextNode(newValue));

  specified = MB_TRUE;
}


//
//Override the set node value member function to create a new TEXT node with
//the String and to add it as the Attribute's child.
//    NOTE:  Not currently impemented, just execute the default setNodeValue
//
void Attr::setNodeValue(const String& nodeValue)
{
  setValue(nodeValue);
}

//
//Return a String represening the value of this node.  If the value is an
//Entity Reference then return the value of the reference.  Otherwise, it is a
//simple conversion of the text value.
//    NOTE: Not currently implemented, just execute the default getNodeValue
//
const String& Attr::getNodeValue()
{
  return getValue();
}


//
//First check to see if the new node is an allowable child for an Attr.  If it
//is, call NodeDefinition's implementation of Insert Before.  If not, return
//null as an error.
//
Node* Attr::insertBefore(Node* newChild, Node* refChild)
{
  Node* returnVal = NULL;

  switch (newChild->getNodeType())
    {
      case Node::TEXT_NODE :
      case Node::ENTITY_REFERENCE_NODE:
        returnVal = NodeDefinition::insertBefore(newChild, refChild);

        if (returnVal)
          specified = MB_TRUE;
        break;
      default:
        returnVal = NULL;
    }

  return returnVal;
}
