/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that invalid tagname updates are handled correctly

const TEST_URL = "data:text/html;charset=utf-8,<div></div>";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);
  yield inspector.markup.expandAll();

  info("Updating the DIV tagname to an invalid value");
  let container = yield focusNode("div", inspector);
  let onCancelReselect = inspector.markup.once("canceledreselectonremoved");
  let tagEditor = container.editor.tag;
  setEditableFieldValue(tagEditor, "<<<", inspector);
  yield onCancelReselect;
  ok(true, "The markup-view emitted the canceledreselectonremoved event");
  is(inspector.selection.nodeFront, container.node,
     "The test DIV is still selected");

  info("Updating the DIV tagname to a valid value this time");
  let onReselect = inspector.markup.once("reselectedonremoved");
  setEditableFieldValue(tagEditor, "span", inspector);
  yield onReselect;
  ok(true, "The markup-view emitted the reselectedonremoved event");

  let spanFront = yield getNodeFront("span", inspector);
  is(inspector.selection.nodeFront, spanFront,
     "The selected node is now the SPAN");
});
