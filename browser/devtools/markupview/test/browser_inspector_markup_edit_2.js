/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that an existing attribute can be modified

const TEST_URL = "data:text/html,<div id='test-div'>Test modifying my ID attribute</div>";

let test = asyncTest(function*() {
  info("Opening the inspector on the test page");
  let {toolbox, inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Selecting the test node");
  let node = content.document.getElementById("test-div");
  yield selectNode(node, inspector);

  info("Verify attributes, only ID should be there for now");
  assertAttributes(node, {
    id: "test-div"
  });

  info("Focus the ID attribute and change its content");
  let editor = getContainerForRawNode(node, inspector).editor;
  let attr = editor.attrs["id"].querySelector(".editable");
  let mutated = inspector.once("markupmutation");
  setEditableFieldValue(attr,
    attr.textContent + ' class="newclass" style="color:green"', inspector);
  yield mutated;

  info("Verify attributes, should have ID, class and style");
  assertAttributes(node, {
    id: "test-div",
    class: "newclass",
    style: "color:green"
  });

  info("Trying to undo the change");
  yield undoChange(inspector);
  assertAttributes(node, {
    id: "test-div"
  });

  yield inspector.once("inspector-updated");
});
