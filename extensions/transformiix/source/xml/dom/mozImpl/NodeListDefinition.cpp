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
//    Implementation of the NodeListDefinition class
//
// Modification History:
// Who  When      What
// TK   03/29/99  Created
//

#include "dom.h"

//
//Create an empty node list.
//
NodeListDefinition::NodeListDefinition()
{
  firstItem = NULL;
  lastItem = NULL;
  length = 0;
}

//
//Free up the memory used by the List of Items.  Don't delete the actual nodes
//though.
//
NodeListDefinition::~NodeListDefinition()
{
  ListItem* pDeleteItem;
  ListItem* pListTraversal = firstItem;

  while (pListTraversal)
    {
    pDeleteItem = pListTraversal;
    pListTraversal = pListTraversal->next;
    delete pDeleteItem;
    }
}

//
//Create a new ListItem, point it to the newNode, and append it to the current
//list of nodes.
//
void NodeListDefinition::append(Node* newNode)
{
  append(*newNode);
}

void NodeListDefinition::append(Node& newNode)
{
  ListItem* newListItem = new ListItem;

  // Setup the new list item
  newListItem->node = &newNode;
  newListItem->prev = lastItem;
  newListItem->next = NULL;

  //Append the list item
  if (lastItem)
    lastItem->next = newListItem;

  lastItem = newListItem;

  //Adjust firstItem if this new item is being added to an empty list
  if (!firstItem)
    firstItem = lastItem;

  //Need to increment the length of the list.  Inherited from NodeList
  length++;
}

//
// Return the Node contained in the item specified
//
Node* NodeListDefinition::item(Int32 index)
{
  Int32 selectLoop;
  ListItem* pListItem = firstItem;

  if (index < length)
    {
      for (selectLoop=0;selectLoop<index;selectLoop++)
        pListItem = pListItem->next;

      return pListItem->node;
    }

  return NULL;
}

//
// Return the number of items in the list
//
Int32 NodeListDefinition::getLength()
{
  return length;
}
