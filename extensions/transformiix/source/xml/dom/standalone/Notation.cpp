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
//    Implementation of the Notation class
//
// Modification History:
// Who  When      What
// TK   03/29/99  Created
//

#include "dom.h"

//
//Construct a text object with the specified document owner and data
//
Notation::Notation(const String& name, const String& pubID,
                   const String& sysID) :
          NodeDefinition(Node::NOTATION_NODE, name, NULL_STRING, NULL)
{
  publicId = pubID;
  systemId = sysID;
}

//
//Return the Public ID of the Notation
//
const String& Notation::getPublicId() const
{
  return publicId;
}

//Return the System ID of the Notation
const String& Notation::getSystemId() const
{
  return systemId;
}

//
//Notation nodes can not have any children, so just return null from all child
//manipulation functions.
//

Node* Notation::insertBefore(Node* newChild, Node* refChild)
{
  return NULL;
}

Node* Notation::replaceChild(Node* newChild, Node* oldChild)
{
  return NULL;
}

Node* Notation::removeChild(Node* oldChild)
{
  return NULL;
}

Node* Notation::appendChild(Node* newChild)
{
  return NULL;
}
