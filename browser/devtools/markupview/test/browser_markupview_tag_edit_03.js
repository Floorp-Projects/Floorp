/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a node's tagname can be edited in the markup-view

const TEST_URL = "data:text/html;charset=utf-8,<div id='retag-me'><div id='retag-me-2'></div></div>";

let test = asyncTest(function*() {
  let {toolbox, inspector} = yield addTab(TEST_URL).then(openInspector);

  yield inspector.markup.expandAll();

  info("Selecting the test node");
  let node = content.document.querySelector("#retag-me");
  let child = content.document.querySelector("#retag-me-2");
  yield selectNode("#retag-me", inspector);

  let container = yield getContainerForSelector("#retag-me", inspector);
  is(node.tagName, "DIV", "We've got #retag-me element, it's a DIV");
  ok(container.expanded, "It is expanded");
  is(child.parentNode, node, "Child #retag-me-2 is inside #retag-me");

  info("Changing the tagname");
  let mutated = inspector.once("markupmutation");
  let tagEditor = container.editor.tag;
  setEditableFieldValue(tagEditor, "p", inspector);
  yield mutated;

  info("Checking that the tagname change was done");
  node = content.document.querySelector("#retag-me");
  container = yield getContainerForSelector("#retag-me", inspector);
  is(node.tagName, "P", "We've got #retag-me, it should now be a P");
  ok(container.expanded, "It is still expanded");
  ok(container.selected, "It is still selected");
  is(child.parentNode, node, "Child #retag-me-2 is still inside #retag-me");
});
