/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

var count = 0;

function trace(msg)
{
  dump("test " + msg + " (" + count + ")\n");
}

function findBody(node)
{
  var children = node.childNodes;
  var length = children.length;
  var child = null;
  var count = 0;
  while (count < length) {
    child = children[count];
    if (child.tagName == "BODY") {
      return child;
    }
    var body = findBody(child);
    if (null != body) {
      return body;
    }
    count++;
  }
  return null;
}

function AppendTest(parent, kidTag, grandKidTag, empty, asWeGo)
{
  trace("enter [" + kidTag + "," + (grandKidTag?grandKidTag:"") + "]");
  var kid = document.createElement(kidTag);
  if (asWeGo) {
    parent.insertBefore(kid, null);
  }
  if (null != grandKidTag) {
    var grandKid = document.createElement(grandKidTag);
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

var inline = document.createElement("SPAN");
body.insertBefore(inline, null);
RunTests(inline, false);
RunTests(inline, true);
