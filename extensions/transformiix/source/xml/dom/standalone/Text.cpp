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
//    Implementation of the Text class
//
// Modification History:
// Who  When      What
// TK   03/29/99  Created
//

#include "dom.h"

//
//Construct a text object with the specified document owner and data
//
Text::Text(const String& theData, Document* owner) :
      CharacterData(Node::TEXT_NODE, "#text", theData, owner)
{
}

//
//Protected constructor for children of the Text Class.  Currently only
//CDATASection needs to use this function.
Text::Text(NodeType type, const String& name, const String& value,
         Document* owner) :
      CharacterData(type, name, value, owner)
{
}


//
//Split the text node at Offset into two siblings.  Return a pointer to the new
//sibling.
//
Text* Text::splitText(Int32 offset)
{
  Text* newTextSibling = NULL;
  String newData;

  if ((offset >= 0) && (offset < nodeValue.length()))
    {
      newTextSibling = getOwnerDocument()->createTextNode(nodeValue.subString(offset, newData));
      getParentNode()->insertBefore(newTextSibling, getNextSibling());
      nodeValue.deleteChars(offset, nodeValue.length() - offset);
    }

  return newTextSibling;
}

//
//Text nodes can not have any children, so just return null from all child
//manipulation functions.
//

Node* Text::insertBefore(Node* newChild, Node* refChild)
{
  return NULL;
}

Node* Text::replaceChild(Node* newChild, Node* oldChild)
{
  return NULL;
}

Node* Text::removeChild(Node* oldChild)
{
  return NULL;
}

Node* Text::appendChild(Node* newChild)
{
  return NULL;
}
