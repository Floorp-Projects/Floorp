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
 * Contributor(s): Tom Kneeland
 *                 Peter Van der Beken <peter.vanderbeken@pandora.be>
 *
 */

/* Implementation of the wrapper class to convert the Mozilla nsIDOMEntity
   interface into a TransforMIIX Entity interface.
*/

#include "mozilladom.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aEntity the nsIDOMElement you want to wrap
 * @param aOwner the document that owns this object
 */
Entity::Entity(nsIDOMEntity* aEntity, Document* aOwner) : Node (aEntity, aOwner)
{
}

/**
 * Destructor
 */
Entity::~Entity()
{
}

/**
 * Call nsIDOMElement::GetPublicId to retrieve the public id for this entity.
 *
 * @return the entity's public id
 */
const String& Entity::getPublicId()
{
    NSI_FROM_TX(Entity)

    publicId.clear();
    if (nsEntity)
        nsEntity->GetPublicId(publicId);
    return publicId;
}

/**
 * Call nsIDOMElement::GetSystemId to retrieve the system id for this entity.
 *
 * @return the entity's system id
 */
const String& Entity::getSystemId()
{
    NSI_FROM_TX(Entity)

    systemId.clear();
    if (nsEntity)
        nsEntity->GetSystemId(systemId);
    return systemId;
}

/**
 * Call nsIDOMElement::GetNotationName to retrieve the notation name for this
 * entity.
 *
 * @return the entity's system id
 */
const String& Entity::getNotationName()
{
    NSI_FROM_TX(Entity)

    notationName.clear();
    if (nsEntity)
        nsEntity->GetNotationName(notationName);
    return notationName;
}
