/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
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
Document::Document() : NodeDefinition(Node::DOCUMENT_NODE, EmptyString(), NULL)
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
