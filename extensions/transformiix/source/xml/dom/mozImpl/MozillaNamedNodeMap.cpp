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

/* Implementation of the wrapper class to convert the Mozilla
   nsIDOMNamedNodeMap interface into a TransforMIIX NamedNodeMap interface.
*/

#include "mozilladom.h"

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
    nsNamedNodeMap = aNamedNodeMap;
}

/**
 * Destructor
 */
NamedNodeMap::~NamedNodeMap()
{
}

/**
 * Wrap a different Mozilla object with this wrapper.
 *
 * @param aNamedNodeMap the nsIDOMNodeList you want to wrap
 */
void NamedNodeMap::setNSObj(nsIDOMNamedNodeMap* aNamedNodeMap)
{
    MozillaObjectWrapper::setNSObj(aNamedNodeMap);
    nsNamedNodeMap = aNamedNodeMap;
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
    nsCOMPtr<nsIDOMNode> node;

    if (NS_SUCCEEDED(nsNamedNodeMap->GetNamedItem(aName.getConstNSString(),
                getter_AddRefs(node))))
        return ownerDocument->createWrapper(node);
    else
        return NULL;
}

/**
 * Call nsIDOMNamedNodeMap::SetNamedItem to add a node to the NamedNodeMap.
 *
 * @param aNode the node to add to the NamedNodeMap
 *
 * @return the node that was added
 */
Node* NamedNodeMap::setNamedItem(Node* aNode)
{
    nsCOMPtr<nsIDOMNode> node;

    if (NS_SUCCEEDED(nsNamedNodeMap->SetNamedItem(aNode->getNSObj(),
                getter_AddRefs(node))))
        return ownerDocument->createWrapper(node);
    else
        return NULL;
}

/**
 * Call nsIDOMNamedNodeMap::RemoveNamedItem to remove a node from the
 * NamedNodeMap.
 *
 * @param aName the name of the node that you want to remove
 *
 * @return the node that was removed
 */
Node* NamedNodeMap::removeNamedItem(const String& aName)
{
    nsCOMPtr<nsIDOMNode> node;

    if (NS_SUCCEEDED(nsNamedNodeMap->RemoveNamedItem(aName.getConstNSString(),
                getter_AddRefs(node))))
        return ownerDocument->createWrapper(node);
    else
        return NULL;
}

/**
 * Call nsIDOMNamedNodeMap::Item to retrieve the node at the given index.
 *
 * @param aIndex the index of the node you want to retrieve
 *
 * @return the node at the given index
 */
Node* NamedNodeMap::item(UInt32 aIndex)
{
    nsCOMPtr<nsIDOMNode> node;

    if (NS_SUCCEEDED(nsNamedNodeMap->Item(aIndex, getter_AddRefs(node))))
        return ownerDocument->createWrapper(node);
    else
        return NULL;
}

/**
 * Get the number of nodes stored in this NamedNodeMap.
 *
 * @return the number of nodes stored in this NamedNodeMap
 */
UInt32 NamedNodeMap::getLength()
{
    UInt32 length = 0;

    nsNamedNodeMap->GetLength(&length);
    return length;
}
