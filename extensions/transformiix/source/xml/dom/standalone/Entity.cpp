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
//    Implementation of the Entity class
//
// Modification History:
// Who  When         What
// TK   03/29/99     Created
// LF   08/06/1999   fixed typo: defalut to default

//

#include "dom.h"

//
//Construct a text object with the specified document owner and data
//
Entity::Entity(const String& name, const String& pubID,
               const String& sysID, const String& notName) :
          NodeDefinition(Node::ENTITY_NODE, name, NULL_STRING, NULL)
{
  publicId = pubID;
  systemId = sysID;
  notationName = notName;
}

//
//Return the Public ID of the Entity
//
const String& Entity::getPublicId() const
{
  return publicId;
}

//
//Return the System ID of the Entity
//
const String& Entity::getSystemId() const
{
  return systemId;
}

//
//Return the Notation Name of the Entity
//
const String& Entity::getNotationName() const
{
  return notationName;
}

//
//First check to see if the new node is an allowable child for an Entity.  If
//it is, call NodeDefinition's implementation of Insert Before.  If not, return
//null as an error.
//
Node* Entity::insertBefore(Node* newChild, Node* refChild)
{
  Node* returnVal = NULL;

  switch (newChild->getNodeType())
    {
      case Node::ELEMENT_NODE:
      case Node::PROCESSING_INSTRUCTION_NODE:
      case Node::COMMENT_NODE:
      case Node::TEXT_NODE :
      case Node::CDATA_SECTION_NODE:
      case Node::ENTITY_REFERENCE_NODE:
        returnVal = NodeDefinition::insertBefore(newChild, refChild);
        break;
      default:
        returnVal = NULL;
    }

  return returnVal;
}
