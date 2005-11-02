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
//    Implementation of the NodeDefinition Class
//
// Modification History:
// Who  When      What
// TK   03/29/99  Created
//

#include "dom.h"

NodeDefinition::NodeDefinition(NodeType type, const String& name,
                               const String& value, Document* owner)
{

  nodeName = name;
  nodeValue = value;
  nodeType = type;

  parentNode = NULL;
  previousSibling = NULL;
  nextSibling = NULL;;
  firstChild = NULL;
  lastChild = NULL;

  ownerDocument = owner;
  length = 0;

}

//
// This node is being destroyed, so loop through and destroy all the children.
// Also, destroy all attributes stored in the attributes NamedNodeMap.
//
NodeDefinition::~NodeDefinition()
{
  Int32 numAttributes = attributes.getLength();
  Int32 killAttrLoop;

  DeleteChildren();

  for (killAttrLoop=0;killAttrLoop<numAttributes;killAttrLoop++)
    delete attributes.removeNamedItem(attributes.item(0)->getNodeName());
}

//
//Remove and delete all children of this node
//
void NodeDefinition::DeleteChildren()
{
  NodeDefinition* pCurrent = firstChild;
  NodeDefinition* pDestroyer;

  while (pCurrent)
    {
      pDestroyer = pCurrent;
      pCurrent = pCurrent->nextSibling;
      delete pDestroyer;
    }

  length = 0;
  firstChild = NULL;
  lastChild = NULL;
}

const String& NodeDefinition::getNodeName() const
{
  return nodeName;
}

const String& NodeDefinition::getNodeValue() const
{
  return nodeValue;
}

const String& NodeDefinition::getNodeValue()
{
  return nodeValue;
}

unsigned short NodeDefinition::getNodeType() const
{
  return nodeType;
}

Node* NodeDefinition::getParentNode() const
{
  return parentNode;
}

NodeList* NodeDefinition::getChildNodes()
{
  return this;
}

Node* NodeDefinition::getFirstChild() const
{
  return firstChild;
}

Node* NodeDefinition::getLastChild() const
{
  return lastChild;
}

Node* NodeDefinition::getPreviousSibling() const
{
  return previousSibling;
}

Node* NodeDefinition::getNextSibling() const
{
  return nextSibling;
}

NamedNodeMap* NodeDefinition::getAttributes()
{
  return &attributes;
}

Document* NodeDefinition::getOwnerDocument() const
{
  return ownerDocument;
}

Node* NodeDefinition::item(Int32 index)
{
  Int32 selectLoop;
  NodeDefinition* pSelectNode = firstChild;

  if (index < length)
    {
      for (selectLoop=0;selectLoop<index;selectLoop++)
        pSelectNode = pSelectNode->nextSibling;

      return pSelectNode;
    }

  return NULL;
}

Int32 NodeDefinition::getLength()
{
  return length;
}

void NodeDefinition::setNodeValue(const String& newNodeValue)
{
  nodeValue = newNodeValue;
}

//
//Insert the "newChild" node before the "refChild" node.  Return a pointer to
//the inserted child.  If the node to insert is a document fragment, then
//insert each child of the document fragment, and return the document fragment
//which should be empty if all the inserts suceeded.
//This function's responsibility is to check for and handle document fragments
//vs. plain nodes.
//     *** NOTE: Need to check the document types before inserting.
//
//               The decision to return the possibly empty document fragment
//               was an implementation choice.  The spec did not dictate what
//               whould occur.
//
Node* NodeDefinition::insertBefore(Node* newChild,
                                   Node* refChild)
{
  NodeDefinition* pCurrentNode = NULL;
  NodeDefinition* pNextNode = NULL;

  //Convert to a NodeDefinition Pointer
  NodeDefinition* pNewChild = (NodeDefinition*)newChild;
  NodeDefinition* pRefChild = (NodeDefinition*)refChild;

  //Check to see if the reference node is a child of this node
  if ((refChild != NULL) && (pRefChild->parentNode != this))
    return NULL;

  if (newChild->getNodeType() == Node::DOCUMENT_FRAGMENT_NODE)
    {
      pCurrentNode = pNewChild->firstChild;
      while (pCurrentNode)
        {
          pNextNode = pCurrentNode->nextSibling;
          pCurrentNode = (NodeDefinition*)pNewChild->removeChild(pCurrentNode);
          implInsertBefore(pCurrentNode, pRefChild);
          pCurrentNode = pNextNode;
        }
      return newChild;
    }
  else
    return implInsertBefore(pNewChild, pRefChild);
}

//
//The code that actually insert one node before another.
//
Node* NodeDefinition::implInsertBefore(NodeDefinition* pNewChild,
                                       NodeDefinition* pRefChild)
{
  //Remove the "newChild" if it is already a child of this node
  if (pNewChild->parentNode == this)
    pNewChild = (NodeDefinition*)removeChild(pNewChild);

  //The new child should not be a child of any other node
  if ((pNewChild->previousSibling == NULL) &&
      (pNewChild->nextSibling == NULL) &&
      (pNewChild->parentNode == NULL))
      {
        if (pRefChild == NULL)
          {
            //Append
            pNewChild->previousSibling = lastChild;

            if (lastChild)
              lastChild->nextSibling = pNewChild;

            lastChild = pNewChild;
          }
        else
          {
            //Insert before the reference node
            if (pRefChild->previousSibling)
              pRefChild->previousSibling->nextSibling = pNewChild;
            pNewChild->nextSibling = pRefChild;
            pNewChild->previousSibling = pRefChild->previousSibling;
            pRefChild->previousSibling = pNewChild;
          }

        pNewChild->parentNode = this;

        if (pNewChild->previousSibling == NULL)
            firstChild = pNewChild;

        length++;

        return pNewChild;
      }

  return NULL;
}


//
//Replace "oldChild" with "newChild".  Return the replaced node, or NULL
//otherwise.
//    *** NOTE:  Need to check that the documents match ***
//
Node* NodeDefinition::replaceChild(Node* newChild,
                                         Node* oldChild)
{
  NodeDefinition* pOldChild = (NodeDefinition*)oldChild;
  NodeDefinition* pNextSibling = NULL;

  //If the newChild is replacing itself then we don't need to do anything
  if (pOldChild == newChild)
      return pOldChild;

  //If "oldChild" is a child of this node, remove it from the list.
  pOldChild = (NodeDefinition*)removeChild(oldChild);

  //If the removal was successful... Else, return null
  if (pOldChild)
    {
      //Try to insert the new node before the old node's next sibling.  If
      //successful, just returned the replaced child.  If not succesful,
      //reinsert the old node, and return NULL.
      pNextSibling = pOldChild->nextSibling;
      if (!insertBefore(newChild, pNextSibling))
        {
        insertBefore(pOldChild, pNextSibling);
        pOldChild = NULL;
        }
    }

  return pOldChild;
}

//
//Remove the specified "oldChild" from this node's children.  First make sure
//the specified node is a child of this node.  Return the removed node, NULL
//otherwise.
//
Node* NodeDefinition::removeChild(Node* oldChild)
{
  NodeDefinition* pOldChild = (NodeDefinition*)oldChild;

  //If "oldChild" is a child of this node, adjust pointers to remove it, and
  //clear "oldChild"'s sibling and parent pointers.
  if (pOldChild->parentNode == this)
    {
      if (pOldChild != firstChild)
        pOldChild->previousSibling->nextSibling = pOldChild->nextSibling;
      else
        firstChild = pOldChild->nextSibling;

      if (pOldChild != lastChild)
        pOldChild->nextSibling->previousSibling = pOldChild->previousSibling;
      else
        lastChild = pOldChild->previousSibling;

      pOldChild->nextSibling = NULL;
      pOldChild->previousSibling = NULL;
      pOldChild->parentNode = NULL;

      length--;

      return pOldChild;
    }

  return NULL;
}

//
//Append a new child node.  First make sure the new child is not already a
//child of another node.  Return the appended node.
//  *** NOTE *** Need to eventually check to make sure the documents match ***
//
Node* NodeDefinition::appendChild(Node* newChild)
{
  return insertBefore(newChild, NULL);
}

Node* NodeDefinition::cloneNode(MBool deep, Node* dest)
{
    return 0;
}

MBool NodeDefinition::hasChildNodes() const
{
  if (firstChild != NULL)
    return MB_TRUE;
  else
    return MB_FALSE;
}
