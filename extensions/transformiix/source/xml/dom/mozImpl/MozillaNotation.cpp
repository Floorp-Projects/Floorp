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
//  Implementation of the wrapper class to convert a Mozilla nsIDOMNotation into
// a TransforMIIX Notation interface.
//
// Modification History:
// Who  When      What

//

#include "mozilladom.h"

//
//Construct a wrapper object using the provided mozilla object and document 
//owner.
//
Notation::Notation(nsIDOMNotation* notation, Document* document) : 
  Node(notation, document)
{
  nsNotation = notation;
}

//
//Destructor.  Do Nothing
//
Notation::~Notation()
{
}

//
//Use this object to wrap another mozilla object
//
void Notation::setNSObj(nsIDOMNotation* notation)
{
  Node::setNSObj(notation);
  nsNotation = notation;
}

//
//Return the Public ID of the Notation
//
const String& Notation::getPublicId() const
{
  nsString* publicId = new nsString();

  if (nsNotation->GetPublicId(*publicId) == NS_OK)
    return *(ownerDocument->createDOMString(publicId));
  else
    {
      delete publicId;
      return NULL_STRING;
    }
}

//Return the System ID of the Notation
const String& Notation::getSystemId() const
{
  nsString* systemId = new nsString();

  if (nsNotation->GetSystemId(*systemId) == NS_OK)
    return *(ownerDocument->createDOMString(systemId));
  else
    {
      delete systemId;
      return NULL_STRING;
    }
}
