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
function makeInline()
{
  var image = document.createElement("IMG");
  image.setAttribute("SRC", "bluedot.gif");
  image.setAttribute("WIDTH", "100");
  image.setAttribute("HEIGHT", "40");
  image.setAttribute("BORDER", "2");
  return image;
}

function makeBlock()
{
  var block = document.createElement("DIV");
  var text = document.createTextNode("Block Text");
  block.appendChild(text);
  return block;
}

function appendInline()
{
  var i = makeInline();
  var it = document.getElementById("it");
  it.appendChild(i);
}

function insertInline(index)
{
  var i = makeInline();
  var it = document.getElementById("it");
  var kids = it.childNodes;
  if ((index < 0) || (index > kids.length)) index = 0;
  var before = kids[index];
  it.insertBefore(i, before);
}

function appendBlock()
{
  var b = makeBlock();
  var it = document.getElementById("it");
  it.appendChild(b);
}

function insertBlock(index)
{
  var b = makeBlock();
  var it = document.getElementById("it");
  var kids = it.childNodes;
  if ((index < 0) || (index > kids.length)) index = 0;
  var before = kids[index];
  it.insertBefore(b, before);
}

function removeAllChildren()
{
  var it = document.getElementById("it");
  var kids = it.childNodes;
  var n = kids.length;
  for (i = 0; i < n; i++) {
   it.removeChild(kids[0]);
  }
}

function removeChild(index)
{
  var it = document.getElementById("it");
  var kids = it.childNodes;
  it.removeChild(kids[index]);
}

function testAppend()
{
  appendInline();  // case 3
  appendBlock();   // case 2
  appendInline();  // case 3
  appendBlock();   // case 1
  appendBlock();   // case 1
  appendInline();  // case 3
  removeAllChildren();
}

function testInsert()
{
  testAppend();

  // first test inserting inlines
  insertInline(0); // case 1
  insertInline(3); // case 2
  insertInline(7); // case 3
  insertInline(8); // case 4

  // test inserting blocks
  insertBlock(0);  // case 2
  insertBlock(1);  // case 3
  insertInline(0);
  insertInline(0);
  insertInline(11);
  insertBlock(13); // case 4
  insertInline(0);
  insertInline(0);
  insertBlock(2);  // case 5

  // Remove all the children before doing block case 1
  removeAllChildren();
  appendInline();
  insertBlock(0);  // case 1

  removeAllChildren();
  appendInline();
  appendInline();
  insertBlock(1);  // case 6

  removeAllChildren();
}

function testRemove()
{
  appendInline();
  removeChild(0);  // case 1

  appendBlock();
  removeChild(0);  // case 2

  appendInline();
  appendBlock();
  appendInline();
  appendBlock();
  appendInline();
  appendBlock();
  appendInline();

  removeChild(3);  // case 3
  removeChild(2);  // case 6
  removeChild(2);  // case 6
  removeChild(1);  // case 5
  removeAllChildren();

  appendInline();
  appendBlock();
  appendInline();
  appendInline();
  appendBlock();
  removeChild(4);  // case 4

  removeAllChildren();
}

function runTest()
{
  dump("Testing inline incremental reflow\n");
  testAppend();
  testInsert();
  testRemove();
}
