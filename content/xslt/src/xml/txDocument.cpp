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
Document::Document() : NodeDefinition(Node::DOCUMENT_NODE, NULL_STRING, NULL)
{
  documentElement = NULL;
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
Element* Document::createElement(const String& tagName)
{
  return new Element(tagName, this);
}

Element* Document::createElementNS(const String& aNamespaceURI,
                                   const String& aTagName)
{
  return new Element(aNamespaceURI, aTagName, this);
}

//
//Construct an attribute with the specified name
//
Attr* Document::createAttribute(const String& name)
{
  return new Attr(name, this);
}

Attr* Document::createAttributeNS(const String& aNamespaceURI,
                                  const String& aName)
{
  return new Attr(aNamespaceURI, aName, this);
}

//
//Construct a text node with the given data
//
Node* Document::createTextNode(const String& theData)
{
  return new NodeDefinition(Node::TEXT_NODE, theData, this);
}

//
//Construct a comment node with the given data
//
Node* Document::createComment(const String& theData)
{
  return new NodeDefinition(Node::COMMENT_NODE, theData, this);
}

//
//Construct a ProcessingInstruction node with the given targe and data.
//
ProcessingInstruction*
  Document::createProcessingInstruction(const String& target,
                                        const String& data)
{
  return new ProcessingInstruction(target, data, this);
}

//
//Return an Element by ID, introduced by DOM2
//
Element* Document::getElementById(const String aID)
{
  /* This would need knowledge of the DTD, and we don't have it.
   * If we knew that we deal with HTML4 or XHTML1 we could check
   * for the "id" attribute, but we don't, so return NULL 
   */
  return NULL;
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

String Document::getBaseURI()
{
  return documentBaseURI;
}

PRInt32 Document::namespaceURIToID(const String& aNamespaceURI)
{
  return txNamespaceManager::getNamespaceID(aNamespaceURI);
}

void Document::namespaceIDToURI(PRInt32 aNamespaceID, String& aNamespaceURI)
{
  aNamespaceURI = txNamespaceManager::getNamespaceURI(aNamespaceID);
  return;
}
