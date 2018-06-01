/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the highlighter is correctly displayed and picker mode is not stopped after
// a shift-click (preview)

const TEST_URI = `data:text/html;charset=utf-8,
                  <p id="one">one</p><p id="two">two</p><p id="three">three</p>`;

add_task(async function() {
  const {inspector, toolbox, testActor} = await openInspectorForURL(TEST_URI);

  const body = await getNodeFront("body", inspector);
  is(inspector.selection.nodeFront, body, "By default the body node is selected");

  info("Start the element picker");
  await startPicker(toolbox);

  info("Shift-clicking element #one should select it but keep the picker ON");
  await clickElement("#one", testActor, inspector, true);
  await checkElementSelected("#one", inspector);
  checkPickerMode(toolbox, true);

  info("Shift-clicking element #two should select it but keep the picker ON");
  await clickElement("#two", testActor, inspector, true);
  await checkElementSelected("#two", inspector);
  checkPickerMode(toolbox, true);

  info("Clicking element #three should select it and turn the picker OFF");
  await clickElement("#three", testActor, inspector, false);
  await checkElementSelected("#three", inspector);
  checkPickerMode(toolbox, false);
});

async function clickElement(selector, testActor, inspector, isShift) {
  const onSelectionChanged = inspector.once("inspector-updated");
  await testActor.synthesizeMouse({
    selector: selector,
    center: true,
    options: { shiftKey: isShift }
  });
  await onSelectionChanged;
}

async function checkElementSelected(selector, inspector) {
  const el = await getNodeFront(selector, inspector);
  is(inspector.selection.nodeFront, el, `The element ${selector} is now selected`);
}

function checkPickerMode(toolbox, isOn) {
  const pickerButton = toolbox.doc.querySelector("#command-button-pick");
  is(pickerButton.classList.contains("checked"), isOn, "The picker mode is correct");
}
