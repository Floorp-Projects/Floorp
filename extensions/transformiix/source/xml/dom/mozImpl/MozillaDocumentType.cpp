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
// Implementation of the wrapper class to convert an nsIDOMDocumentType into a
// TransforMIIX DocumentType interface.
//
// Modification History:
// Who  When      What
//

#include "mozilladom.h"

//
//Construct a text object with the specified document owner and data
//
DocumentType::DocumentType(nsIDOMDocumentType* documentType, Document* owner):
  Node(documentType, owner)
{
  nsDocumentType = documentType;
}

//
//Destructor.  Do nothing
//
DocumentType::~DocumentType()
{
}

//
//Use this wrapper object to wrap a different Mozilla object
//
void DocumentType::setNSObj(nsIDOMDocumentType* documentType)
{
  Node::setNSObj(documentType);
  nsDocumentType = documentType;
}

//
//Retrieve the name from the Mozilla object and wrap acordingly.
//
const String& DocumentType::getName() const
{
  nsString* name = new nsString();

  if (nsDocumentType->GetName(*name) == NS_OK)
    return *(ownerDocument->createDOMString(name));
  else
    {
      delete name;
      return NULL_STRING;
    }
}

//
//Retrieve the entites from the Mozilla object and wrap acordingly.
//
NamedNodeMap* DocumentType::getEntities()
{
  nsIDOMNamedNodeMap* tmpEntities = NULL;

  if (nsDocumentType->GetEntities(&tmpEntities) == NS_OK)
    return ownerDocument->createNamedNodeMap(tmpEntities);
  else
    return NULL;
}

//
//Retrieve the notations from the Mozilla object and wrap acordingly
//
NamedNodeMap* DocumentType::getNotations()
{
  nsIDOMNamedNodeMap* notations = NULL;

  if (nsDocumentType->GetNotations(&notations) == NS_OK)
    return ownerDocument->createNamedNodeMap(notations);
  else
    return NULL;
}
