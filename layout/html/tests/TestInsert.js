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

var count = 0;

function trace(msg)
{
  dump("test " + msg + " (" + count + ")\n");
}

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

function TestInsert(parent, child)
{
  var childTag = "(text)";
  if (child.getNodeType() != Node.TEXT) {
    childTag = child.getTagName();
  }

  // Insert a piece of text before the span; this tests insertion at the
  // beginning
  trace("insert before [" + parent.getTagName() + "," + childTag + "]");
  count++;
  var beforeText = document.createTextNode("before ");
  parent.insertBefore(beforeText, child);

  // Insert a piece of text before the span; after the above, this tests
  // insertion in the middle.
  trace("insert middle [" + parent.getTagName() + "," + childTag + "]");
  count++;
  parent.insertBefore(document.createTextNode("middle "), child);
}

var body = findBody(document.documentElement);

// Create a block child with a span in it
var block = document.createElement("P", null);
var span = document.createElement("SPAN", null);
var spanText = document.createTextNode("Some text");
span.insertBefore(spanText, null);
block.insertBefore(span, null);

// Append the block to the body
body.insertBefore(block, null);

// Insert stuff into the block
TestInsert(block, span);

// Insert stuff into the span
TestInsert(span, spanText);
