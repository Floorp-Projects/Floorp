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
// Tom Kneeland (1/31/2000)
//
//  Wrapper class to convert a Mozilla nsIDOMNamedNodeMap into a TransforMIIX
//  NamedNodeMap object.
//
// Modification History:
// Who  When      What
//

#include "mozilladom.h"

//
//Construct the wrapper object
//
NamedNodeMap::NamedNodeMap(nsIDOMNamedNodeMap* namedNodeMap, Document* owner)
{
  nsNamedNodeMap = namedNodeMap;
  ownerDocument = owner;
}

//
//Destroy the wrapper object leaving the nsIDOMNamedNodeMap object intact.
//
NamedNodeMap::~NamedNodeMap()
{
}

//
// Search for the Node using the specified name.  If found, refer to the owning
// document for a wrapper for the returned nsIDOMNode obejct
// (nsIDOMNamedNodeMap::GetNamedItem)
//
Node* NamedNodeMap::getNamedItem(const String& name)
{
  nsIDOMNode* node = NULL;

  if (nsNamedNodeMap->GetNamedItem(name.getConstNSString(), &node) == NS_OK)
    return ownerDocument->createWrapper(node);
  else
    return NULL;
}

//
// Store the nsIDOMNode object wrapped by arg in the nsIDOMNamedNodeMap.  If
// successful refer to the owning document for a wrapper for the resultant
// nsIDOMNode.  (In theory this should produce exactly the same wrapper class
// as arg).  
// (nsIDOMNamedNodeMap::SetNamedItem)
//
Node* NamedNodeMap::setNamedItem(Node* arg)
{
  nsIDOMNode* node = NULL;

  if (nsNamedNodeMap->SetNamedItem(arg->getNSObj(), &node) == NS_OK)
    return ownerDocument->createWrapper(node);
  else
    return NULL;
}

//
// Remove the named item from the nsIDOMNamedNodeMap.  If successful, refer to
// the owner documetn for a wrapper object for the result.
// (nsIDOMNamedNodeMap::removeNamedItem)
//
Node* NamedNodeMap::removeNamedItem(const String& name)
{
  nsIDOMNode* node = NULL;

  if (nsNamedNodeMap->RemoveNamedItem(name.getConstNSString(), &node) == NS_OK)
    return ownerDocument->createWrapper(node);
  else
    return NULL;
}

//
// Retrieve the Node specified by index, then ask the owning document to provide
// a wrapper object for it. ( nsIDOMNamedNodeMap::Item )
//
Node* NamedNodeMap::item(UInt32 index)
{
  nsIDOMNode* node = NULL;

  if (nsNamedNodeMap->Item(index, &node) == NS_OK)
    return ownerDocument->createWrapper(node);
  else
    return NULL;
}

//
// Get the number of Nodes stored in this list (nsIDOMNamedNodeMap::GetLength())
//
UInt32 NamedNodeMap::getLength()
{
  UInt32 length = 0;

  nsNamedNodeMap->GetLength(&length);

  return length;
} 
