/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

// Find the BODY element
function findBody(node)
{
  // XXX A better way to do this would be to use getElementsByTagName(), but
  // it isn't implemented yet...
  if (node.getTagName() == "BODY") {
    return node;
  }

  var children = node.getChildNodes()
  return findBody(children.getNextNode())
}

// Given the body element, find the first table element
function findTable(body)
{
  // XXX A better way to do this would be to use getElementsByTagName(), but
  // it isn't implemented yet...
  var children = body.getChildNodes()
  var child = children.getNextNode()
  while (child) {
    if (child.getNodeType() == 2) {
      if (child.getTagName() == "TABLE") {
        break;
      }
    }

    child = children.getNextNode()
  }

  return child;	
}

// Change the table's caption
function changeCaption(table)
{
  // Get the first element. This is the caption (maybe). We really should
  // check...
  var caption = table.getFirstChild()

  // Get the caption text
  var text = caption.getFirstChild()

  // Append some text
  text.append(" NEW TEXT")
}

changeCaption(findTable(findBody(document.documentElement)))

