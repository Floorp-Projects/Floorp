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

/* Implementation of the wrapper class to convert the Mozilla nsIDOMDocumentType
   interface into a TransforMIIX DocumentType interface.
*/

#include "mozilladom.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aDocumentType the nsIDOMDocumentType you want to wrap
 * @param aOwner the document that owns this object
 */
DocumentType::DocumentType(nsIDOMDocumentType* aDocumentType, Document* aOwner):
        Node(aDocumentType, aOwner)
{
    nsDocumentType = aDocumentType;
}

/**
 * Destructor
 */
DocumentType::~DocumentType()
{
}

/**
 * Wrap a different Mozilla object with this wrapper.
 *
 * @param aDocumentType the nsIDOMDocumentType you want to wrap
 */
void DocumentType::setNSObj(nsIDOMDocumentType* aDocumentType)
{
    Node::setNSObj(aDocumentType);
    nsDocumentType = aDocumentType;
}

/**
 * Call nsIDOMDocumentType::GetName to get the name of the document type.
 *
 * @return the name of the document type
 */
const String& DocumentType::getName()
{
    nodeName.clear();
    nsDocumentType->GetName(nodeName.getNSString());
    return nodeName;
}

/**
 * Call nsIDOMDocumentType::GetEntities to get the entities of the document
 * type.
 *
 * @return the entities of the document type
 */
NamedNodeMap* DocumentType::getEntities()
{
    nsCOMPtr<nsIDOMNamedNodeMap> tmpEntities;

    if (NS_SUCCEEDED(nsDocumentType->GetEntities(getter_AddRefs(tmpEntities))))
        return (NamedNodeMap*)ownerDocument->createNamedNodeMap(tmpEntities);
    else
        return NULL;
}

/**
 * Call nsIDOMDocumentType::GetNotations to get the notations of the document
 * type.
 *
 * @return the notations of the document type
 */
NamedNodeMap* DocumentType::getNotations()
{
    nsCOMPtr<nsIDOMNamedNodeMap> notations;

    if (NS_SUCCEEDED(nsDocumentType->GetNotations(getter_AddRefs(notations))))
        return (NamedNodeMap*)ownerDocument->createNamedNodeMap(notations);
    else
        return NULL;
}
