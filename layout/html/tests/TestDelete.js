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

function dump2(msg, parent, child)
{
  dump(msg);
  dump(" parent=");
  dump(parent.tagName);
  dump(" child=");
  dump((child.nodeType != Node.TEXT_NODE)  ? child.tagName : "(text)");

  dump(" kids: ");
  var children = parent.childNodes;
  var length = children.length;
  var child = null;
  var count = 0;
  while (count < length) {
    child = children.item(count);
    dump((child.nodeType != Node.TEXT_NODE)  ? child.tagName : "(text)");
    dump(",");
    count++;
  }

  dump("\n");
}


function firstChildOf(node)
{
  return node.childNodes.item(0);
}

function middleChildOf(node)
{
  var children = node.childNodes;
  return children.item(Math.floor(children.length / 2));
}

function lastChildOf(node)
{
  var children = node.childNodes;
  return children.item(children.length - 1)
}

function findContainer(node, name)
{
  dump("Looking in " + node.tagName + " for " + name + "\n");
  var children = node.childNodes;
  var length = children.length;
  var child = null;
  var count = 0;
  while (count < length) {
    child = children.item(count);
    if (child.nodeType != Node.TEXT_NODE) {
      if (child.tagName == name) {
        return child;
      }
      var body = findContainer(child, name);
      if (null != body) {
        return body;
      }
    }
    count++;
  }
  return null;
}

var longText = "Lots more text (this text is long so that we can verify that continuations are handled properly when they are deleted) ";

function makeTestDocument(node)
{
  var blockTag = new Array(6);
  blockTag[0] = "DIV";
  blockTag[1] = "ADDRESS";
  blockTag[2] = "BLOCKQUOTE";
  blockTag[3] = "CENTER";
  blockTag[4] = "H1";
  blockTag[5] = "H2";

  var spanTag = new Array(6);
  spanTag[0] = "SPAN";
  spanTag[1] = "B";
  spanTag[2] = "I";
  spanTag[3] = "EM";
  spanTag[4] = "STRONG";
  spanTag[5] = "TT";

  for (b = 0; b < 6; b++) {
    var block = document.createElement(blockTag[b]);
    block.insertBefore(document.createTextNode("Opening text ["), null);
    for (s = 0; s < 6; s++) {
      var span = document.createElement(spanTag[s]);
      block.insertBefore(span, null);
      span.insertBefore(document.createTextNode("More text, "), null);
      span.insertBefore(document.createTextNode("And more text, "), null);
      span.insertBefore(document.createTextNode("And even more text. "), null);
      span.insertBefore(document.createTextNode(longText), null);
      span.insertBefore(document.createTextNode("Because more text "), null);
      span.insertBefore(document.createTextNode("Is more text."), null);
    }
    block.insertBefore(document.createTextNode("] Closing text"), null);

    // Now put that entire tree into the body
    node.insertBefore(block, null);
  }
}

// delete first, last, middle
function testDelete(node)
{
  var kid = firstChildOf(node);
  dump2("Remove first child", node, kid);
  node.removeChild(kid);

  kid = lastChildOf(node);
  dump2("Remove last child", node, kid);
  node.removeChild(kid);

  kid = middleChildOf(node);
  dump2("Remove middle child", node, kid);
  node.removeChild(kid);
}

var body = findContainer(document.documentElement, "BODY");
makeTestDocument(body);

//var span = findContainer(body, "SPAN");
//testDelete(span);

var block = findContainer(body, "DIV");
//testDelete(block);

// delete children of the body (cuz its a pseudo)
testDelete(body);
