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
//    Implementation of the Document class
//
// Modification History:
// Who  When        What
// TK   03/29/99    Created
// LF   08/06/1999  Removed Default argument initializer from
//                  Document() constructor
// LF   08/06/1999  fixed typo: defalut to default
//

#include "dom.h"

//
//Construct a Document.  Currently no parameters are required, but the the
//node constructor is called to identify the node type.
//
Document::Document() : NodeDefinition(Node::DOCUMENT_NODE, nsString(), NULL)
{
  mIDMap.Init(0);
  documentElement = nsnull;
}

//
//Return the one and only element for this document
//
Element* Document::getDocumentElement()
{
  return documentElement;
}

//
//Construct an empty document fragment.
//    NOTE:  The caller is responsible for cleaning up this fragment's memory
//           when it is no longer needed.
//
Node* Document::createDocumentFragment()
{
  return new DocumentFragment(this);
}

//
//Construct an element with the specified tag name.
//    NOTE:  The caller is responsible for cleaning up the element's menory
//
Element* Document::createElement(const nsAString& tagName)
{
  return new Element(tagName, this);
}

Element* Document::createElementNS(const nsAString& aNamespaceURI,
                                   const nsAString& aTagName)
{
  return new Element(aNamespaceURI, aTagName, this);
}

//
//Construct an attribute with the specified name
//
Attr* Document::createAttribute(const nsAString& name)
{
  return new Attr(name, this);
}

Attr* Document::createAttributeNS(const nsAString& aNamespaceURI,
                                  const nsAString& aName)
{
  return new Attr(aNamespaceURI, aName, this);
}

//
//Construct a text node with the given data
//
Node* Document::createTextNode(const nsAString& theData)
{
  return new NodeDefinition(Node::TEXT_NODE, theData, this);
}

//
//Construct a comment node with the given data
//
Node* Document::createComment(const nsAString& theData)
{
  return new NodeDefinition(Node::COMMENT_NODE, theData, this);
}

//
//Construct a ProcessingInstruction node with the given targe and data.
//
ProcessingInstruction*
  Document::createProcessingInstruction(const nsAString& target,
                                        const nsAString& data)
{
  return new ProcessingInstruction(target, data, this);
}

//
//Return an Element by ID, introduced by DOM2
//
DHASH_WRAPPER(txIDMap, txIDEntry, nsAString&)

Element* Document::getElementById(const nsAString& aID)
{
  txIDEntry* entry = mIDMap.GetEntry(aID);
  if (entry)
    return entry->mElement;
  return nsnull;
}

/**
 * private setter for element ID
 */
PRBool
Document::setElementID(const nsAString& aID, Element* aElement)
{
  txIDEntry* id = mIDMap.AddEntry(aID);
  // make sure IDs are unique
  if (id->mElement) {
    return PR_FALSE;
  }
  id->mElement = aElement;
  id->mElement->setIDValue(aID);
  return PR_TRUE;
}

Node* Document::appendChild(Node* newChild)
{
  unsigned short nodeType = newChild->getNodeType();

  // Convert to a NodeDefinition Pointer
  NodeDefinition* pNewChild = (NodeDefinition*)newChild;

  if (pNewChild->parentNode == this)
    {
      pNewChild = implRemoveChild(pNewChild);
      if (nodeType == Node::ELEMENT_NODE)
        documentElement = nsnull;
    }

  switch (nodeType)
    {
      case Node::PROCESSING_INSTRUCTION_NODE :
      case Node::COMMENT_NODE :
      case Node::DOCUMENT_TYPE_NODE :
        return implAppendChild(pNewChild);

      case Node::ELEMENT_NODE :
        if (!documentElement)
          {
            Node* returnVal = implAppendChild(pNewChild);
            documentElement = (Element*)pNewChild;
            return returnVal;
          }

      default:
        break;
    }

  return nsnull;
}

nsresult Document::getBaseURI(nsAString& aURI)
{
  aURI = documentBaseURI;
  return NS_OK;
}

PRInt32 Document::namespaceURIToID(const nsAString& aNamespaceURI)
{
  return txNamespaceManager::getNamespaceID(aNamespaceURI);
}

void Document::namespaceIDToURI(PRInt32 aNamespaceID, nsAString& aNamespaceURI)
{
  txNamespaceManager::getNamespaceURI(aNamespaceID, aNamespaceURI);
}
