/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the node picker can pick elements with pointer-events: none when holding Shift.

const TEST_URI = `data:text/html;charset=utf-8,
  <!DOCTYPE html>
  <main style="display:flex">
    <div id="pointer">Regular element</div>
    <div id="nopointer" style="pointer-events: none">Element with pointer-events: none</div>
    <div id="transluscent" style="pointer-events: none;opacity: 0.1">Element with opacity of 0.1</div>
    <div id="invisible" style="pointer-events: none;opacity: 0">Element with opacity of 0</div>
  </main>`;
const IS_OSX = Services.appinfo.OS === "Darwin";

add_task(async function () {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URI);

  const body = await getNodeFront("body", inspector);
  is(
    inspector.selection.nodeFront,
    body,
    "By default the body node is selected"
  );

  info("Start the element picker");
  await startPicker(toolbox);

  info(
    "Shift-clicking element with pointer-events: none does select the element"
  );
  await clickElement({
    inspector,
    selector: "#nopointer",
    shiftKey: true,
  });
  await checkElementSelected("#nopointer", inspector);

  info("Shift-clicking element with default pointer-events value also works");
  await clickElement({
    inspector,
    selector: "#pointer",
    shiftKey: true,
  });
  await checkElementSelected("#pointer", inspector);

  info(
    "Clicking element with pointer-events: none without holding Shift won't select the element but its parent"
  );
  await clickElement({
    inspector,
    selector: "#nopointer",
    shiftKey: false,
  });
  await checkElementSelected("main", inspector);

  info("Shift-clicking transluscent visible element works");
  await clickElement({
    inspector,
    selector: "#transluscent",
    shiftKey: true,
  });
  await checkElementSelected("#transluscent", inspector);

  info("Shift-clicking invisible element select its parent");
  await clickElement({
    inspector,
    selector: "#invisible",
    shiftKey: true,
  });
  await checkElementSelected("main", inspector);
});

async function clickElement({ selector, inspector, shiftKey }) {
  const onSelectionChanged = inspector.once("inspector-updated");
  await safeSynthesizeMouseEventAtCenterInContentPage(selector, {
    shiftKey,
    // Hold meta/ctrl so we don't have to start the picker again
    [IS_OSX ? "metaKey" : "ctrlKey"]: true,
  });
  await onSelectionChanged;
}

async function checkElementSelected(selector, inspector) {
  const el = await getNodeFront(selector, inspector);
  is(
    inspector.selection.nodeFront,
    el,
    `The element ${selector} is now selected`
  );
}
