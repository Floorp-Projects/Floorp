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
//    Implementation of the DocumentType class
//
// Modification History:
// Who  When      What
// TK   03/29/99  Created
//

#include "dom.h"

//
//Construct a text object with the specified document owner and data
//
DocumentType::DocumentType(const String& name, NamedNodeMap* theEntities,
                           NamedNodeMap* theNotations) :
              NodeDefinition(Node::DOCUMENT_TYPE_NODE, name, NULL_STRING, NULL)
{
  entities = theEntities;
  notations = theNotations;
}

//
//When destroying the DocumentType, the entities and notations must be
//destroyed too.
//
DocumentType::~DocumentType()
{
  if (entities)
    delete entities;

  if (notations)
    delete notations;
}

//
//Return a pointer to the entities contained in this Document Type
//
NamedNodeMap* DocumentType::getEntities()
{
  return entities;
}

//
//Return a pointer to the notations contained in this Document Type
//
NamedNodeMap* DocumentType::getNotations()
{
  return notations;
}

//
//Comment nodes can not have any children, so just return null from all child
//manipulation functions.
//

Node* DocumentType::insertBefore(Node* newChild, Node* refChild)
{
  return NULL;
}

Node* DocumentType::replaceChild(Node* newChild, Node* oldChild)
{
  return NULL;
}

Node* DocumentType::removeChild(Node* oldChild)
{
  return NULL;
}

Node* DocumentType::appendChild(Node* newChild)
{
  return NULL;
}
