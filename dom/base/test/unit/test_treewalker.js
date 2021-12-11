/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  test_treeWalker_currentNode();
}

// TEST CODE

function test_treeWalker_currentNode() {
  var XHTMLDocString = '<html xmlns="http://www.w3.org/1999/xhtml">';
  XHTMLDocString += "<body><input/>input</body></html>";

  var doc = ParseXML(XHTMLDocString);

  var body = doc.getElementsByTagName("body")[0];
  var filter = NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT;
  var walker = doc.createTreeWalker(body, filter, null);
  walker.currentNode = body.firstChild;
  walker.nextNode();
}
