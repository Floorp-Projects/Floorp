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

function testInsert(text)
{
  text.deleteData(0, 99999);

  // test insert into empty string
  text.insertData(0, "word");
  if (text.data != "word") {
    dump("testInsert Error(0): '" + text.data + "' != word\n");
  }

  // test insert into middle of string
  text.insertData(1, "Z");
  if (text.data != "wZord") {
    dump("testInsert Error(1): '" + text.data + "' != wZord\n");
  }
  text.insertData(4, "Z");
  if (text.data != "wZorZd") {
    dump("testInsert Error(2): '" + text.data + "' != wZorZd\n");
  }

  // test insert at end of string (== append)
  text.insertData(99, "Z");
  if (text.data != "wZorZdZ") {
    dump("testInsert Error(3): '" + text.data + "' != wZorZdZ\n");
  }

  // test insert at start with illegal offset
  text.insertData(-9000, "first");
  if (text.data != "firstwZorZdZ") {
    dump("testInsert Error(4): '" + text.data + "' != firstwZorZdZ\n");
  }
}

function testAppend(text)
{
  text.deleteData(0, 99999);

  // test append into empty string
  text.appendData("word");
  if (text.data != "word") {
    dump("testAppend Error(0): '" + text.data + "' != word\n");
  }

  // test append onto a non-empty string
  text.appendData("word");
  if (text.data != "wordword") {
    dump("testAppend Error(1): '" + text.data + "' != wordword\n");
  }
}

function testDelete(text)
{
  text.deleteData(0, 99999);
  text.appendData("wordwo3rd");

  // test delete at start of word
  text.deleteData(0, 4);
  if (text.data != "wo3rd") {
    dump("testDelete Error(0): '" + text.data + "' != wo3rd\n");
  }

  // test delete in middle of word
  text.deleteData(2, 1);
  if (text.data != "word") {
    dump("testDelete Error(1): '" + text.data + "' != word\n");
  }

  // test delete at end of word
  text.deleteData(2, 2);
  if (text.data != "wo") {
    dump("testDelete Error(2): '" + text.data + "' != wo\n");
  }

  // test delete of remaining data
  text.deleteData(0, 2);
  if (text.data != "") {
    dump("testDelete Error(3): '" + text.data + "' != ''\n");
  }

  // test invalid delete's
  text.appendData("word");
  text.deleteData(-100, -10);
  if (text.data != "word") {
    dump("testDelete Error(4): '" + text.data + "' != word\n");
  }
  text.deleteData(-100, 0);
  if (text.data != "word") {
    dump("testDelete Error(5): '" + text.data + "' != word\n");
  }
  text.deleteData(0, -10);
  if (text.data != "word") {
    dump("testDelete Error(6): '" + text.data + "' != word\n");
  }
  text.deleteData(0, 0);
  if (text.data != "word") {
    dump("testDelete Error(7): '" + text.data + "' != word\n");
  }
}

function testReplace(text)
{
  text.deleteData(0, 99999);
  text.appendData("word");

  // replace nothing with something
  text.replaceData(0, 0, "fish");
  if (text.data != "fishword") {
    dump("testReplace Error(0): '" + text.data + "' != fisword\n");
  }

  // replace something with nothing
  text.replaceData(0, 4, "");
  if (text.data != "word") {
    dump("testReplace Error(1): '" + text.data + "' != word\n");
  }

  // replace nothing with something
  text.replaceData(1, 1, "fish");
  if (text.data != "wfishrd") {
    dump("testReplace Error(2): '" + text.data + "' != wfishrd\n");
  }

  // replace something with nothing
  text.replaceData(1, 4, "");
  if (text.data != "wrd") {
    dump("testReplace Error(3): '" + text.data + "' != wrd\n");
  }

  // replace something with something
  text.replaceData(0, 1, "W");
  text.replaceData(1, 1, "O");
  text.replaceData(2, 1, "R");
  text.replaceData(3, 1, "D");
  if (text.data != "WORD") {
    dump("testReplace Error(4): '" + text.data + "' != WORD\n");
  }

  // replace nothing with something
  text.replaceData(4, 97, "FISH");
  if (text.data != "WORDFISH") {
    dump("testReplace Error(5): '" + text.data + "' != WORDFISH\n");
  }

  // test illegal count's/offset for replace
  text.replaceData(-99, 4, "SWORD");
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
    var children = container.childNodes;
    var length = children.length;
    var child = null;
    var count = 0;
    while (count < length) {
      child = children[count];
      if (child.nodeType == Node.TEXT_NODE) {
        return child;
      }
      var text = findText(child);
      if (null != text) {
        return text;
      }
      count++;
    }
  }
  return null;
}

function findBody(node)
{
  if (node.nodeType != Node.ELEMENT_NODE) {
    return null;
  }
  var children = node.childNodes;
  if (children == null) {
    return null;
  }
  var length = children.length;
  var child = null;
  var count = 0;
  while (count < length) {
    child = children[count];
    if (child.tagName == "BODY") {
      dump("BODY found");
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

var body = findBody(document.documentElement)
var text = findText(body)
testText(text)
dump("Test finished\n");
