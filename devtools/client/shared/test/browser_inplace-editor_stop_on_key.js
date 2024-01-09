/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_inplace_editor.js */

"use strict";

loadHelperScript("helper_inplace_editor.js");

// Test the inplace-editor stopOnX options behavior

add_task(async function () {
  await addTab("data:text/html;charset=utf-8,inline editor stopOnX");
  const { host, doc } = await createHost();

  testStopOnReturn(doc);
  testStopOnTab(doc);
  testStopOnShiftTab(doc);

  host.destroy();
  gBrowser.removeCurrentTab();
});

function testStopOnReturn(doc) {
  const { span1El, span2El } = setupMarkupAndCreateInplaceEditors(doc);

  info(`Create an editable field with "stopOnReturn" set to true`);
  editableField({
    element: span1El,
    focusEditableFieldAfterApply: true,
    focusEditableFieldContainerSelector: "main",
    stopOnReturn: true,
  });
  editableField({
    element: span2El,
  });

  info("Activate inplace editor on first span");
  span1El.click();

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-1",
    "Visible editable field is the one associated with first span"
  );
  assertFocusedElementInplaceEditorInput(doc);

  info("Press Return");
  EventUtils.synthesizeKey("VK_RETURN");

  is(
    doc.activeElement.id,
    "span-1",
    "Using Enter did not advance the editor to the next focusable element"
  );

  info("Activate inplace editor on first span again");
  span1El.click();

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-1",
    "Visible editable field is the one associated with first span"
  );
  assertFocusedElementInplaceEditorInput(doc);

  const isMacOS = Services.appinfo.OS === "Darwin";
  info(`Press ${isMacOS ? "Cmd" : "Ctrl"} + Enter`);
  EventUtils.synthesizeKey("VK_RETURN", {
    [isMacOS ? "metaKey" : "ctrlKey"]: true,
  });

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-2",
    `Using ${
      isMacOS ? "Cmd" : "Ctrl"
    } + Enter did advance the editor to the next focusable element`
  );
}

function testStopOnTab(doc) {
  const { span1El, span2El } = setupMarkupAndCreateInplaceEditors(doc);

  info(`Create editable fields with "stopOnTab" set to true`);
  const options = {
    focusEditableFieldAfterApply: true,
    focusEditableFieldContainerSelector: "main",
    stopOnTab: true,
  };
  editableField({
    element: span1El,
    ...options,
  });
  editableField({
    element: span2El,
    ...options,
  });

  info("Activate inplace editor on first span");
  span1El.click();

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-1",
    "Visible editable field is the one associated with first span"
  );
  assertFocusedElementInplaceEditorInput(doc);

  info("Press Tab");
  EventUtils.synthesizeKey("VK_TAB");

  is(
    doc.activeElement.id,
    "span-1",
    "Using Tab did not advance the editor to the next focusable element"
  );

  info(
    "Activate inplace editor on second span to check that Shift+Tab does work"
  );
  span2El.click();

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-2",
    "Visible editable field is the one associated with second span"
  );
  assertFocusedElementInplaceEditorInput(doc);

  info("Press Shift+Tab");
  EventUtils.synthesizeKey("VK_TAB", {
    shiftKey: true,
  });

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-1",
    `Using Shift + Tab did move the editor to the previous focusable element`
  );
}

function testStopOnShiftTab(doc) {
  const { span1El, span2El } = setupMarkupAndCreateInplaceEditors(doc);
  info(`Create editable fields with "stopOnShiftTab" set to true`);
  const options = {
    focusEditableFieldAfterApply: true,
    focusEditableFieldContainerSelector: "main",
    stopOnShiftTab: true,
  };
  editableField({
    element: span1El,
    ...options,
  });
  editableField({
    element: span2El,
    ...options,
  });

  info("Activate inplace editor on second span");
  span2El.click();

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-2",
    "Visible editable field is the one associated with second span"
  );
  assertFocusedElementInplaceEditorInput(doc);

  info("Press Shift+Tab");
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });

  is(
    doc.activeElement.id,
    "span-2",
    "Using Shift+Tab did not move the editor to the previous focusable element"
  );

  info(
    "Activate inplace editor on first span to check that Tab is not impacted"
  );
  span1El.click();

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-1",
    "Visible editable field is the one associated with first span"
  );
  assertFocusedElementInplaceEditorInput(doc);

  info("Press Tab");
  EventUtils.synthesizeKey("VK_TAB");

  is(
    doc.activeElement.inplaceEditor.elt.id,
    "span-2",
    `Using Tab did move the editor to the next focusable element`
  );
}

function setupMarkupAndCreateInplaceEditors(doc) {
  // For some reason <button> or <input> are not rendered, so let's use divs with
  // tabindex attribute to make them focusable.
  doc.body.innerHTML = `
      <main>
          <span id="span-1" tabindex=0>SPAN 1</span>
          <span id="span-2" tabindex=0>SPAN 2</span>
      <main>`;

  const span1El = doc.getElementById("span-1");
  const span2El = doc.getElementById("span-2");
  return { span1El, span2El };
}

function assertFocusedElementInplaceEditorInput(doc) {
  ok(
    doc.activeElement.matches("input.styleinspector-propertyeditor"),
    "inplace editor input is focused"
  );
}
