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

/* Implementation of the wrapper class to convert the Mozilla nsIDOMNodeList
   interface into a TransforMIIX NodeList interface.
*/

#include "mozilladom.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aNodeList the nsIDOMNodeList you want to wrap
 * @param aOwner the document that owns this object
 */
NodeList::NodeList(nsIDOMNodeList* aNodeList, Document* aOwner) :
        MozillaObjectWrapper(aNodeList, aOwner)
{
    nsNodeList = aNodeList;
}

/**
 * Destructor
 */
NodeList::~NodeList()
{
}

/**
 * Wrap a different Mozilla object with this wrapper.
 *
 * @param aNodeList the nsIDOMNodeList you want to wrap
 */
void NodeList::setNSObj(nsIDOMNodeList* aNodeList)
{
    MozillaObjectWrapper::setNSObj(aNodeList);
    nsNodeList = aNodeList;
}

/**
 * Call nsIDOMNodeList::Item to the child at the specified index.
 *
 * @param aIndex the index of the child you want to get
 *
 * @return the child at the given index or NULL if there is none
 */
Node* NodeList::item(UInt32 aIndex)
{
    nsCOMPtr<nsIDOMNode> node;

    if (NS_SUCCEEDED(nsNodeList->Item(aIndex, getter_AddRefs(node))))
        return ownerDocument->createWrapper(node);
    else
        return NULL;
}

/**
 * Call nsIDOMNodeList::GetLength to get the number of nodes.
 *
 * @return the number of nodes
 */
UInt32 NodeList::getLength()
{
    UInt32 length = 0;

    nsNodeList->GetLength(&length);
    return length;
}
