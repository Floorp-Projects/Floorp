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

/**
 * Implementation of the wrapper class to convert the Mozilla
 * nsIDOMNamedNodeMap interface into a TransforMIIX NamedNodeMap interface.
 */

#include "mozilladom.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNode.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aNamedNodeMap the nsIDOMNamedNodeMap you want to wrap
 * @param aOwner the document that owns this object
 */
NamedNodeMap::NamedNodeMap(nsIDOMNamedNodeMap* aNamedNodeMap,
                           Document* aOwner) :
        MozillaObjectWrapper(aNamedNodeMap, aOwner)
{
}

/**
 * Destructor
 */
NamedNodeMap::~NamedNodeMap()
{
}

/**
 * Call nsIDOMNamedNodeMap::GetNamedItem to get the node with the specified
 * name.
 *
 * @param aName the name to look for
 *
 * @return the node with the specified name
 */
Node* NamedNodeMap::getNamedItem(const String& aName)
{
    NSI_FROM_TX(NamedNodeMap);
    nsCOMPtr<nsIDOMNode> node;
    nsNamedNodeMap->GetNamedItem(aName, getter_AddRefs(node));
    if (!node) {
        return nsnull;
    }
    return mOwnerDocument->createWrapper(node);
}

/**
 * Call nsIDOMNamedNodeMap::Item to retrieve the node at the given index.
 *
 * @param aIndex the index of the node you want to retrieve
 *
 * @return the node at the given index
 */
Node* NamedNodeMap::item(PRUint32 aIndex)
{
    NSI_FROM_TX(NamedNodeMap);
    nsCOMPtr<nsIDOMNode> node;
    nsNamedNodeMap->Item(aIndex, getter_AddRefs(node));
    if (!node) {
        return nsnull;
    }
    return mOwnerDocument->createWrapper(node);
}

/**
 * Get the number of nodes stored in this NamedNodeMap.
 *
 * @return the number of nodes stored in this NamedNodeMap
 */
PRUint32 NamedNodeMap::getLength()
{
    NSI_FROM_TX(NamedNodeMap);
    PRUint32 length = 0;
    nsNamedNodeMap->GetLength(&length);
    return length;
}
