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
// Tom Kneeland (02/02/2000)
//
// Implementation of the wrapper class to convert a Mozilla nsIDOMEntity into
// a TransforMIIX Entity interface.
//
// Modification History:
// Who  When         What
//

#include "mozilladom.h"

//
//Construct an entity wrapper
//
Entity::Entity(nsIDOMEntity* entity, Document* owner) : Node (entity, owner)
{
  nsEntity = entity;
}

//
//Destructor.  Do nothing
//
Entity::~Entity()
{
}

//
//Wrap a different mozilla object
//
void Entity::setNSObj(nsIDOMEntity* entity)
{
  Node::setNSObj(entity);
  nsEntity = entity;
}

//
//Retrieve the public id from the mozilla object, and then retrieve the 
//appropriate wrapper from the document.
//
const String& Entity::getPublicId() const
{
  nsString* publicId = new nsString();

  if (nsEntity->GetPublicId(*publicId) == NS_OK)
    return *(ownerDocument->createDOMString(publicId));
  else
    {
      delete publicId;
      return NULL_STRING;
    }
}

//
//Retrieve the system id from the Mozilla object, and then wrap it appropriately
//
const String& Entity::getSystemId() const
{
  nsString* systemId = new nsString();

  if (nsEntity->GetSystemId(*systemId) == NS_OK)
    return *(ownerDocument->createDOMString(systemId));
  else
    {
      delete systemId;
      return NULL_STRING;
    }
}

//
//Retrieve the notation name from the Mozilla object, and then wrap it
//appropriately
//
const String& Entity::getNotationName() const
{
  nsString* notationName = new nsString();

  if (nsEntity->GetNotationName(*notationName) == NS_OK)
    return *(ownerDocument->createDOMString(notationName));
  else
    {
      delete notationName;
      return NULL_STRING;
    }
}
