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
var newTextCount = 1;
function NewText() {
  return document.createTextNode("Some text #" + newTextCount++);
}

function NewImage() {
  var img = document.createElement("img");
  if (img) {
    img.src = "blue-square.gif";
    img.width = "50";
    img.height = "50";
  }
  return img;
}

function NewComment() {
  return document.createComment("Another pesky comment");
}

function NewInline() {
  var i = document.createElement("i");
  if (i) {
    i.appendChild(NewText());
    i.appendChild(NewComment());
    i.appendChild(NewText());
  }
  return i;
}

function NewBlock() {
  var b = document.createElement("p");
  if (b) {
    b.appendChild(NewText());
    b.appendChild(NewComment());
    b.appendChild(NewText());
  }
  return b;
}

function NewButton(msg,func) {
  var b = document.createElement("input");
  if (b) {
    b.type = "button";
    b.value = msg;
    b.onclick = func;
  }
  return b;
}

function InsertIt(ctor) {
  var list = document.getElementsByName("it");
  if (list) {
    for (var i = 0; i < list.length; i++) {
      var node = list[i];
      if (node) {
        node.insertBefore(ctor(), node.childNodes[0]);
      }
    }
  }
}

function AppendIt(ctor) {
  var list = document.getElementsByName("it");
  if (list) {
    for (var i = 0; i < list.length; i++) {
      var node = list[i];
      if (node) {
        node.appendChild(ctor());
      }
    }
  }
}

function InsertText() { InsertIt(NewText); }
function AppendText() { AppendIt(NewText); }
function InsertImage() { InsertIt(NewImage); }
function AppendImage() { AppendIt(NewImage); }
function InsertComment() { InsertIt(NewComment); }
function AppendComment() { AppendIt(NewComment); }
function InsertBlock() { InsertIt(NewBlock); }
function AppendBlock() { AppendIt(NewBlock); }
function InsertInline() { InsertIt(NewInline); }
function AppendInline() { AppendIt(NewInline); }

function FindText(n) {
  var kids = n.childNodes;
  for (var i = 0; i < kids.length; i++) {
    var kid = kids[i];
    if (kid.nodeType == Node.TEXT_NODE) {
      return kid;
	}
  }
  return null;
}

function ShrinkText() {
  var list = document.getElementsByName("it");
  if (list) {
    for (var i = 0; i < list.length; i++) {
      var text = FindText(list[i]);
      if (text) {
        text.deleteData(0, 1);
      }
    }
  }
}

function GrowText() {
  var list = document.getElementsByName("it");
  if (list) {
    for (var i = 0; i < list.length; i++) {
      var text = FindText(list[i]);
      if (text) {
        text.insertData(0, "Zz");
      }
    }
  }
}

function RemoveNode(n) {
  var list = document.getElementsByName("it");
  if (list) {
    for (var i = 0; i < list.length; i++) {
      var node = list[i];
      if (node && node.childNodes[n]) {
        node.removeChild(node.childNodes[n]);
      }
    }
  }
}

function RemoveNodeN() {
  var list = document.getElementsByName("it");
  if (list) {
    for (var i = 0; i < list.length; i++) {
      var node = list[i];
      if (node && node.childNodes.length) {
        node.removeChild(node.childNodes[node.childNodes.length-1]);
      }
    }
  }
}

function RemoveNode0() { RemoveNode(0); }
function RemoveNode1() { RemoveNode(1); }
function RemoveNode2() { RemoveNode(2); }
function RemoveNode3() { RemoveNode(3); }

var seed = 0;
function Rand() {
  seed = (seed * 69069 + 12359) & 0x7fffffff;
  return seed;
}

var tests = new Array(17);
tests[0] = InsertText;
tests[1] = AppendText;
tests[2] = InsertComment;
tests[3] = AppendComment;
tests[4] = InsertImage;
tests[5] = AppendImage;
tests[6] = InsertBlock;
tests[7] = AppendBlock;
tests[8] = InsertInline;
tests[9] = AppendInline;
tests[10] = ShrinkText;
tests[11] = GrowText;
tests[12] = RemoveNode0;
tests[13] = RemoveNode1;
tests[14] = RemoveNode2;
tests[15] = RemoveNode3;
tests[16] = RemoveNodeN;

function RandomTests() {
  for (var i = 0; i < 100; i++) {
    var rr = Rand() >> 3;
    var ix = rr % tests.length;
    dump(ix + "\n");
    var f = tests[ix];
    f();
  }
}
