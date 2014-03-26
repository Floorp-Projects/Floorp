/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test editing a node's text content

const TEST_URL = TEST_URL_ROOT + "doc_markup_edit.html";

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Expanding all nodes");
  yield inspector.markup.expandAll();

  let node = getNode(".node6").firstChild;
  is(node.nodeValue, "line6", "The test node's text content is correct");

  info("Changing the text content");

  info("Listening to the markupmutation event");
  let onMutated = inspector.once("markupmutation");
  let editor = getContainerForRawNode(node, inspector).editor;
  let field = editor.elt.querySelector("pre");
  setEditableFieldValue(field, "New text", inspector);
  yield onMutated;

  is(node.nodeValue, "New text", "Test test node's text content has changed");

  yield inspector.once("inspector-updated");
});
