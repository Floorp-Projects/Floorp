/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    child = children.item(count);
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

function TestInsert(parent, child)
{
  var childTag = "(text)";
  if (child.nodeType != Node.TEXT_NODE) {
    childTag = child.tagName;
  }

  // Insert a piece of text before the span; this tests insertion at the
  // beginning
  trace("insert before [" + parent.tagName + "," + childTag + "]");
  count++;
  var beforeText = document.createTextNode("before ");
  parent.insertBefore(beforeText, child);

  // Insert a piece of text before the span; after the above, this tests
  // insertion in the middle.
  trace("insert middle [" + parent.tagName + "," + childTag + "]");
  count++;
  parent.insertBefore(document.createTextNode("middle "), child);
}

var body = findBody(document.documentElement);

// Create a block child with a span in it
var block = document.createElement("P");
var span = document.createElement("SPAN");
var spanText = document.createTextNode("Some text");
span.insertBefore(spanText, null);
block.insertBefore(span, null);

// Append the block to the body
body.insertBefore(block, null);

// Insert stuff into the block
TestInsert(block, span);

// Insert stuff into the span
TestInsert(span, spanText);
