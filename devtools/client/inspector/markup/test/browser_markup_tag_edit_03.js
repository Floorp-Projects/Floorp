/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a node's tagname can be edited in the markup-view

const TEST_URL = `data:text/html;charset=utf-8,
                  <div id='retag-me'><div id='retag-me-2'></div></div>`;

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);

  yield inspector.markup.expandAll();

  info("Selecting the test node");
  yield focusNode("#retag-me", inspector);

  info("Getting the markup-container for the test node");
  let container = yield getContainerForSelector("#retag-me", inspector);
  ok(container.expanded, "The container is expanded");

  let parentInfo = yield testActor.getNodeInfo("#retag-me");
  is(parentInfo.tagName.toLowerCase(), "div",
     "We've got #retag-me element, it's a DIV");
  is(parentInfo.numChildren, 1, "#retag-me has one child");
  let childInfo = yield testActor.getNodeInfo("#retag-me > *");
  is(childInfo.attributes[0].value, "retag-me-2",
     "#retag-me's only child is #retag-me-2");

  info("Changing #retag-me's tagname in the markup-view");
  let mutated = inspector.once("markupmutation");
  let tagEditor = container.editor.tag;
  setEditableFieldValue(tagEditor, "p", inspector);
  yield mutated;

  info("Checking that the markup-container exists and is correct");
  container = yield getContainerForSelector("#retag-me", inspector);
  ok(container.expanded, "The container is still expanded");
  ok(container.selected, "The container is still selected");

  info("Checking that the tagname change was done");
  parentInfo = yield testActor.getNodeInfo("#retag-me");
  is(parentInfo.tagName.toLowerCase(), "p",
     "The #retag-me element is now a P");
  is(parentInfo.numChildren, 1, "#retag-me still has one child");
  childInfo = yield testActor.getNodeInfo("#retag-me > *");
  is(childInfo.attributes[0].value, "retag-me-2",
     "#retag-me's only child is #retag-me-2");
});
