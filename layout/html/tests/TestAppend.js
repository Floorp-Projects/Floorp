/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

function findBody(node)
{
  var children = node.getChildNodes();
  var length = children.getLength();
  var child = children.getNextNode();
  var count = 0;
  while (count < length) {
    if (child.getTagName() == "BODY") {
      return child;
    }
    var body = findBody(child);
    if (null != body) {
      return body;
    }
    child = children.getNextNode();
    count++;
  }
  return null;
}

function AppendABlock(node, tag, text)
{
  var block2 = document.createElement("P", null);
  var block3 = document.createElement(tag, null);
  var text1 = document.createTextNode(text);
  block3.insertBefore(text1, null);
  block2.insertBefore(block3, null);
  node.insertBefore(block2, null);
}

// Append an empty block to the end of the body
var body = findBody(document.documentElement)
var block = document.createElement("P", null);
body.insertBefore(block, null);

// Append a block with something in it to the body
AppendABlock(body, "B", "Old bold text");

// Append an inline container with something in it to the body; then
// append a piece of text after that; do this 10 times.
for (i = 0; i < 10; i++) {
  var block4 = document.createElement("I", null);
  var text2 = document.createTextNode(i + " Italic text");
  block4.insertBefore(text2, null);
  body.insertBefore(block4, null);
  var text3 = document.createTextNode(" (regular text) ");
  body.insertBefore(text3, null);
}

// Append a block with something in it to the body
AppendABlock(body, "B", "New bold text");
