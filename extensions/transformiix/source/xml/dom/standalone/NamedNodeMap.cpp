/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
//    Implementation of the NamedNodeMap class
//

#include "dom.h"

NamedNodeMap::NamedNodeMap()
{
}

NamedNodeMap::~NamedNodeMap()
{
}

Node* NamedNodeMap::getNamedItem(const String& name)
{
  ListItem* pSearchItem = findListItemByName(name);

  if (pSearchItem)
    return pSearchItem->node;
  else
    return NULL;
}

Node* NamedNodeMap::setNamedItem(Node* arg)
{
  //Since the DOM does not specify any ording for the NamedNodeMap, just
  //try and remove the new node (arg).  If successful, return a pointer to
  //the removed item.  Reguardless of wheter the node already existed or not,
  //insert the new node at the end of the list.
  Node* pReplacedNode = removeNamedItem(arg->getNodeName());

  NodeListDefinition::append(arg);

  return pReplacedNode;
}

Node* NamedNodeMap::removeNamedItem(const String& name)
{
  NodeListDefinition::ListItem* pSearchItem;
  Node* returnNode;

  pSearchItem = findListItemByName(name);

  if (pSearchItem)
    {
      if (pSearchItem != firstItem)
        pSearchItem->prev->next = pSearchItem->next;
      else
        firstItem = pSearchItem->next;

      if (pSearchItem != lastItem)
        pSearchItem->next->prev = pSearchItem->prev;
      else
        lastItem = pSearchItem->prev;

      pSearchItem->next = NULL;
      pSearchItem->prev = NULL;

      length--;
      returnNode = pSearchItem->node;
      delete pSearchItem;

      return returnNode;
    }


  return NULL;
}

NodeListDefinition::ListItem*
  NamedNodeMap::findListItemByName(const String& name)
{
  NodeListDefinition::ListItem* pSearchItem = firstItem;

  while (pSearchItem)
    {
      if (name.isEqual(pSearchItem->node->getNodeName()))
        return pSearchItem;

      pSearchItem = pSearchItem->next;
    }

  return NULL;
}

AttrMap::AttrMap()
{
    ownerElement = NULL;
}

AttrMap::~AttrMap()
{
}

Node* AttrMap::setNamedItem(Node* arg)
{
  if (arg->getNodeType() != Node::ATTRIBUTE_NODE)
    return NULL;
  ((Attr*)arg)->ownerElement = ownerElement;
  return NamedNodeMap::setNamedItem(arg);
}

Node* AttrMap::removeNamedItem(const String& name)
{
  Attr* node = (Attr*)NamedNodeMap::removeNamedItem(name);
  if (node)
    node->ownerElement = NULL;
  return node;
}
