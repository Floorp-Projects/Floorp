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

function AppendTest(parent, kidTag, grandKidTag, empty, asWeGo)
{
  trace("enter [" + kidTag + "," + (grandKidTag?grandKidTag:"") + "]");
  var kid = document.createElement(kidTag, null);
  if (asWeGo) {
    parent.insertBefore(kid, null);
  }
  if (null != grandKidTag) {
    var grandKid = document.createElement(grandKidTag, null);
    if (empty) {
      kid.insertBefore(grandKid, null);
    }
    else {
      if (asWeGo) {
        kid.insertBefore(grandKid, null);
      }
      var text = document.createTextNode("inner text");
      grandKid.insertBefore(text, null);
      if (!asWeGo) {
        kid.insertBefore(grandKid, null);
      }
    }
  }
  if (!asWeGo) {
    parent.insertBefore(kid, null);
  }
  trace("exit [" + kidTag + "," + (grandKidTag?grandKidTag:"") + "]");
  count++;
}

// Append empty block to a X
function RunTests(parent, asWeGo)
{
  // Append empty block to a X
  AppendTest(parent, "P", null, true, asWeGo);

  // Append empty inline to a X
  AppendTest(parent, "I", null, true, asWeGo);

  // Append non-empty block to a X
  AppendTest(parent, "P", "P", false, asWeGo);
  AppendTest(parent, "P", "I", false, asWeGo);

  // Append non-empty inline to a X
  AppendTest(parent, "I", "P", false, asWeGo);
  AppendTest(parent, "I", "TT", false, asWeGo);
}

var body = findBody(document.documentElement);

RunTests(body, false);
RunTests(body, true);

var inline = document.createElement("SPAN", null);
body.insertBefore(inline, null);
RunTests(inline, false);
RunTests(inline, true);
