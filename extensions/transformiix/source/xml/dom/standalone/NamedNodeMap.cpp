/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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

Node* NamedNodeMap::getNamedItem(const nsAString& name)
{
  ListItem* pSearchItem = findListItemByName(name);

  if (pSearchItem)
    return pSearchItem->node;
  else
    return nsnull;
}

Node* NamedNodeMap::setNamedItem(Node* arg)
{
  //Since the DOM does not specify any ording for the NamedNodeMap, just
  //try and remove the new node (arg).  If successful, return a pointer to
  //the removed item.  Reguardless of wheter the node already existed or not,
  //insert the new node at the end of the list.
  nsAutoString nodeName;
  arg->getNodeName(nodeName);
  Node* pReplacedNode = removeNamedItem(nodeName);

  NodeListDefinition::append(arg);

  return pReplacedNode;
}

Node* NamedNodeMap::removeNamedItem(const nsAString& name)
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

      pSearchItem->next = nsnull;
      pSearchItem->prev = nsnull;

      length--;
      returnNode = pSearchItem->node;
      delete pSearchItem;

      return returnNode;
    }


  return nsnull;
}

NodeListDefinition::ListItem*
  NamedNodeMap::findListItemByName(const nsAString& name)
{
  NodeListDefinition::ListItem* pSearchItem = firstItem;

  while (pSearchItem)
    {
      nsAutoString nodeName;
      pSearchItem->node->getNodeName(nodeName);
      if (name.Equals(nodeName))
        return pSearchItem;

      pSearchItem = pSearchItem->next;
    }

  return nsnull;
}

AttrMap::AttrMap()
{
    ownerElement = nsnull;
}

AttrMap::~AttrMap()
{
}

Node* AttrMap::setNamedItem(Node* arg)
{
  if (arg->getNodeType() != Node::ATTRIBUTE_NODE)
    return nsnull;
  ((Attr*)arg)->ownerElement = ownerElement;
  return NamedNodeMap::setNamedItem(arg);
}

Node* AttrMap::removeNamedItem(const nsAString& name)
{
  Attr* node = (Attr*)NamedNodeMap::removeNamedItem(name);
  if (node)
    node->ownerElement = nsnull;
  return node;
}

void AttrMap::clear()
{
  ListItem* pDeleteItem;
  ListItem* pListTraversal = firstItem;

  while (pListTraversal) {
    pDeleteItem = pListTraversal;
    pListTraversal = pListTraversal->next;
    delete pDeleteItem->node;
    delete pDeleteItem;
  }
  firstItem = 0;
  lastItem = 0;
  length = 0;
}
