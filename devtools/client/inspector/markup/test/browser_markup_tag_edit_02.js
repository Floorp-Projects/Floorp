/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that an existing attribute can be modified

const TEST_URL = `data:text/html,
                  <div id='test-div'>Test modifying my ID attribute</div>`;

add_task(async function() {
  info("Opening the inspector on the test page");
  const {inspector, testActor} = await openInspectorForURL(TEST_URL);

  info("Selecting the test node");
  await focusNode("#test-div", inspector);

  info("Verify attributes, only ID should be there for now");
  await assertAttributes("#test-div", {
    id: "test-div"
  }, testActor);

  info("Focus the ID attribute and change its content");
  const {editor} = await getContainerForSelector("#test-div", inspector);
  const attr = editor.attrElements.get("id").querySelector(".editable");
  const mutated = inspector.once("markupmutation");
  setEditableFieldValue(attr,
    attr.textContent + ' class="newclass" style="color:green"', inspector);
  await mutated;

  info("Verify attributes, should have ID, class and style");
  await assertAttributes("#test-div", {
    id: "test-div",
    class: "newclass",
    style: "color:green"
  }, testActor);

  info("Trying to undo the change");
  await undoChange(inspector);
  await assertAttributes("#test-div", {
    id: "test-div"
  }, testActor);
});
