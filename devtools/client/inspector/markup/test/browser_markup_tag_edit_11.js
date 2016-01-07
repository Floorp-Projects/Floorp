/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Bug 1090874 - Tests that a node is not recreated when it's tagname editor
// is blurred and no changes were done.

const TEST_URL = "data:text/html;charset=utf-8,<div></div>";

add_task(function*() {
  let isEditTagNameCalled = false;

  let {toolbox, inspector} = yield addTab(TEST_URL).then(openInspector);

  // Overriding the editTagName walkerActor method here to check that it isn't
  // called when blurring the tagname field.
  inspector.walker.editTagName = function() { isEditTagNameCalled = true; }

  yield selectNode("div", inspector);
  let container = yield getContainerForSelector("div", inspector);
  let tagEditor = container.editor.tag;

  info("Blurring the tagname field");
  tagEditor.blur();
  is(isEditTagNameCalled, false, "The editTagName method wasn't called");

  info("Updating the tagname to uppercase");
  setEditableFieldValue(tagEditor, "DIV", inspector);
  is(isEditTagNameCalled, false, "The editTagName method wasn't called");

  info("Updating the tagname to a different value");
  setEditableFieldValue(tagEditor, "SPAN", inspector);
  is(isEditTagNameCalled, true, "The editTagName method was called");
});