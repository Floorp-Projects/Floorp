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
Document::Document(DocumentType* theDoctype) :
          NodeDefinition(Node::DOCUMENT_NODE, "#document", NULL_STRING, NULL)
{
  documentElement = NULL;
  doctype = theDoctype;
}

//
//Return the one and only element for this document
//
Element* Document::getDocumentElement()
{
  return documentElement;
}

//
//Return the document type of this document object
//
DocumentType* Document::getDoctype()
{
  return doctype;
}

//
//Return a constant reference to the DOM's Implementation
//
const DOMImplementation& Document::getImplementation()
{
  return implementation;
}

//
//Ensure that no Element node is inserted if the document already has an
//associated Element child.
//
Node* Document::insertBefore(Node* newChild, Node* refChild)
{
  Node* returnVal = NULL;

  NodeDefinition* pCurrentNode = NULL;
  NodeDefinition* pNextNode = NULL;

  //Convert to a NodeDefinition Pointer
  NodeDefinition* pNewChild = (NodeDefinition*)newChild;
  NodeDefinition* pRefChild = (NodeDefinition*)refChild;

  //Check to see if the reference node is a child of this node
  if ((refChild != NULL) && (pRefChild->getParentNode() != this))
    return NULL;

  switch (pNewChild->getNodeType())
    {
      case Node::DOCUMENT_FRAGMENT_NODE :
        pCurrentNode = (NodeDefinition*)pNewChild->getFirstChild();
        while (pCurrentNode)
          {
            pNextNode = (NodeDefinition*)pCurrentNode->getNextSibling();

            //Make sure that if the current node is an Element, the document
            //doesn't already have one.
            if ((pCurrentNode->getNodeType() != Node::ELEMENT_NODE) ||
                ((pCurrentNode->getNodeType() == Node::ELEMENT_NODE) &&
                  (documentElement == NULL)))
              {
                pCurrentNode = (NodeDefinition*)pNewChild->removeChild(pCurrentNode);
                implInsertBefore(pCurrentNode, pRefChild);

                if (pCurrentNode->getNodeType() == Node::ELEMENT_NODE)
                  documentElement = (Element*)pCurrentNode;
              }
            pCurrentNode = pNextNode;
          }
        returnVal = newChild;
        break;

      case Node::PROCESSING_INSTRUCTION_NODE :
      case Node::COMMENT_NODE :
      case Node::DOCUMENT_TYPE_NODE :
        returnVal = implInsertBefore(pNewChild, pRefChild);
        break;

      case Node::ELEMENT_NODE :
        if (!documentElement)
          {
          documentElement = (Element*)pNewChild;
          returnVal = implInsertBefore(pNewChild, pRefChild);
          }
        else
          returnVal = NULL;
        break;
      default:
        returnVal =  NULL;
    }

  return returnVal;
}

//
//Ensure that if the newChild is an Element and the Document already has an
//element, then oldChild should be specifying the existing element.  If not
//then the replacement can not take place.
//
Node* Document::replaceChild(Node* newChild, Node* oldChild)
{
  Node* replacedChild = NULL;

  if (newChild->getNodeType() != Node::ELEMENT_NODE)
    {
      //The new child is not an Element, so perform replacement
      replacedChild = NodeDefinition::replaceChild(newChild, oldChild);

      //If old node was an Element, then the document's element has been
      //replaced with a non-element node.  Therefore clear the documentElement
      //pointer
      if (replacedChild && (oldChild->getNodeType() == Node::ELEMENT_NODE))
        documentElement = NULL;

      return replacedChild;
    }
  else
    {
      //A node is being replaced with an Element.  If the document does not
      //have an elemet yet, then just allow the replacemetn to take place.
      if (!documentElement)
        replacedChild = NodeDefinition::replaceChild(newChild, oldChild);
      else if (oldChild->getNodeType() == Node::ELEMENT_NODE)
        replacedChild = NodeDefinition::replaceChild(newChild, oldChild);

      if (replacedChild)
        documentElement = (Element*)newChild;

      return replacedChild;
    }
}

//
//Update the documentElement pointer if the associated Element node is being
//removed.
//
Node* Document::removeChild(Node* oldChild)
{
  Node* removedChild = NULL;

  removedChild = NodeDefinition::removeChild(oldChild);

  if (removedChild && (removedChild->getNodeType() == Node::ELEMENT_NODE))
    documentElement = NULL;

  return removedChild;
}

//
//Construct an empty document fragment.
//    NOTE:  The caller is responsible for cleaning up this fragment's memory
//           when it is no longer needed.
//
DocumentFragment* Document::createDocumentFragment()
{
  return new DocumentFragment("#document-fragment", NULL_STRING, this);
}

//
//Construct an element with the specified tag name.
//    NOTE:  The caller is responsible for cleaning up the element's menory
//
Element* Document::createElement(const String& tagName)
{
  return new Element(tagName, this);
}

//
//Construct an attribute with the specified name
//
Attr* Document::createAttribute(const String& name)
{
  return new Attr(name, this);
}

//
//Construct a text node with the given data
//
Text* Document::createTextNode(const String& theData)
{
  return new Text(theData, this);
}

//
//Construct a comment node with the given data
//
Comment* Document::createComment(const String& theData)
{
  return new Comment(theData, this);
}

//
//Construct a CDATASection node with the given data
//
CDATASection* Document::createCDATASection(const String& theData)
{
  return new CDATASection(theData, this);
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
//Construct an EntityReference with the given name
//
EntityReference* Document::createEntityReference(const String& name)
{
  return new EntityReference(name, this);
}
