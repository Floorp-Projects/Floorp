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

function testInsert(text)
{
  text.remove(0, 99999);

  // test insert into empty string
  text.insert(0, "word");
  if (text.data != "word") {
    dump("testInsert Error(0): '" + text.data + "' != word\n");
  }

  // test insert into middle of string
  text.insert(1, "Z");
  if (text.data != "wZord") {
    dump("testInsert Error(1): '" + text.data + "' != wZord\n");
  }
  text.insert(4, "Z");
  if (text.data != "wZorZd") {
    dump("testInsert Error(2): '" + text.data + "' != wZorZd\n");
  }

  // test insert at end of string (== append)
  text.insert(99, "Z");
  if (text.data != "wZorZdZ") {
    dump("testInsert Error(3): '" + text.data + "' != wZorZdZ\n");
  }

  // test insert at start with illegal offset
  text.insert(-9000, "first");
  if (text.data != "firstwZorZdZ") {
    dump("testInsert Error(4): '" + text.data + "' != firstwZorZdZ\n");
  }
}

function testAppend(text)
{
  text.remove(0, 99999);

  // test append into empty string
  text.append("word");
  if (text.data != "word") {
    dump("testAppend Error(0): '" + text.data + "' != word\n");
  }

  // test append onto a non-empty string
  text.append("word");
  if (text.data != "wordword") {
    dump("testAppend Error(1): '" + text.data + "' != wordword\n");
  }
}

function testDelete(text)
{
  text.remove(0, 99999);
  text.append("wordwo3rd");

  // test delete at start of word
  text.remove(0, 4);
  if (text.data != "wo3rd") {
    dump("testDelete Error(0): '" + text.data + "' != wo3rd\n");
  }

  // test delete in middle of word
  text.remove(2, 1);
  if (text.data != "word") {
    dump("testDelete Error(1): '" + text.data + "' != word\n");
  }

  // test delete at end of word
  text.remove(2, 2);
  if (text.data != "wo") {
    dump("testDelete Error(2): '" + text.data + "' != wo\n");
  }

  // test delete of remaining data
  text.remove(0, 2);
  if (text.data != "") {
    dump("testDelete Error(3): '" + text.data + "' != ''\n");
  }

  // test invalid delete's
  text.append("word");
  text.remove(-100, -10);
  if (text.data != "word") {
    dump("testDelete Error(4): '" + text.data + "' != word\n");
  }
  text.remove(-100, 0);
  if (text.data != "word") {
    dump("testDelete Error(5): '" + text.data + "' != word\n");
  }
  text.remove(0, -10);
  if (text.data != "word") {
    dump("testDelete Error(6): '" + text.data + "' != word\n");
  }
  text.remove(0, 0);
  if (text.data != "word") {
    dump("testDelete Error(7): '" + text.data + "' != word\n");
  }
}

function testReplace(text)
{
  text.remove(0, 99999);
  text.append("word");

  // replace nothing with something
  text.replace(0, 0, "fish");
  if (text.data != "fishword") {
    dump("testReplace Error(0): '" + text.data + "' != fisword\n");
  }

  // replace something with nothing
  text.replace(0, 4, "");
  if (text.data != "word") {
    dump("testReplace Error(1): '" + text.data + "' != word\n");
  }

  // replace nothing with something
  text.replace(1, 1, "fish");
  if (text.data != "wfishrd") {
    dump("testReplace Error(2): '" + text.data + "' != wfishrd\n");
  }

  // replace something with nothing
  text.replace(1, 4, "");
  if (text.data != "wrd") {
    dump("testReplace Error(3): '" + text.data + "' != wrd\n");
  }

  // replace something with something
  text.replace(0, 1, "W");
  text.replace(1, 1, "O");
  text.replace(2, 1, "R");
  text.replace(3, 1, "D");
  if (text.data != "WORD") {
    dump("testReplace Error(4): '" + text.data + "' != WORD\n");
  }

  // replace nothing with something
  text.replace(4, 97, "FISH");
  if (text.data != "WORDFISH") {
    dump("testReplace Error(5): '" + text.data + "' != WORDFISH\n");
  }

  // test illegal count's/offset for replace
  text.replace(-99, 4, "SWORD");
  if (text.data != "SWORDFISH") {
    dump("testReplace Error(6): '" + text.data + "' != SWORDFISH\n");
  }
}

function testText(text)
{
  testInsert(text);
  testAppend(text);
  testDelete(text);
  testReplace(text);
}

function findText(container)
{
  if (container.hasChildNodes()) {
    // Find the first piece of text in the container or one of it's
    // children and return it
    var children = container.getChildNodes();
    var length = children.getLength();
    var child = children.getNextNode();
    var count = 0;
    while (count < length) {
      if (child.getNodeType() == Node.TEXT) {
        return child;
      }
      var text = findText(child);
      if (null != text) {
        return text;
      }
      child = children.getNextNode();
      count++;
    }
  }
  return null;
}

function findBody(node)
{
  if (node.getTagName() == "BODY") {
    return node;
  }
  var children = node.getChildNodes();
  return findBody(children.getNextNode());
}

var body = findBody(document.documentElement)
var text = findText(body)
testText(text)
dump("Test finished\n");
