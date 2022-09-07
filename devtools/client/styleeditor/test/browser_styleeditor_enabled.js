/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that style sheets can be disabled and enabled.

// https rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

add_task(async function() {
  const { panel, ui } = await openStyleEditorForURL(TESTCASE_URI);
  const editor = await ui.editors[0].getSourceEditor();

  const summary = editor.summary;
  const stylesheetToggle = summary.querySelector(".stylesheet-toggle");
  ok(stylesheetToggle, "stylesheet toggle button exists");

  is(
    editor.styleSheet.disabled,
    false,
    "first stylesheet is initially enabled"
  );

  is(
    summary.classList.contains("disabled"),
    false,
    "first stylesheet is initially enabled, UI does not have DISABLED class"
  );

  info("Disabling the first stylesheet.");
  await toggleEnabled(editor, stylesheetToggle, panel.panelWindow);

  is(editor.styleSheet.disabled, true, "first stylesheet is now disabled");
  is(
    summary.classList.contains("disabled"),
    true,
    "first stylesheet is now disabled, UI has DISABLED class"
  );

  info("Enabling the first stylesheet again.");
  await toggleEnabled(editor, stylesheetToggle, panel.panelWindow);

  is(
    editor.styleSheet.disabled,
    false,
    "first stylesheet is now enabled again"
  );
  is(
    summary.classList.contains("disabled"),
    false,
    "first stylesheet is now enabled again, UI does not have DISABLED class"
  );
});

add_task(async function testSystemStylesheet() {
  const { ui } = await openStyleEditorForURL("about:support");

  const aboutSupportEditor = ui.editors.find(
    editor => editor.friendlyName === "aboutSupport.css"
  );
  ok(!!aboutSupportEditor, "Found the editor for aboutSupport.css");
  const aboutSupportToggle = aboutSupportEditor.summary.querySelector(
    ".stylesheet-toggle"
  );
  ok(aboutSupportToggle, "enabled toggle button exists");
  ok(!aboutSupportToggle.disabled, "enabled toggle button is not disabled");
  is(
    aboutSupportToggle.getAttribute("tooltiptext"),
    "Toggle style sheet visibility"
  );

  const formsEditor = ui.editors.find(
    editor => editor.friendlyName === "forms.css"
  );
  ok(!!formsEditor, "Found the editor for forms.css");
  const formsToggle = formsEditor.summary.querySelector(".stylesheet-toggle");
  ok(formsToggle, "enabled toggle button exists");
  ok(formsToggle.disabled, "enabled toggle button is disabled");
  is(
    formsToggle.getAttribute("tooltiptext"),
    "System style sheets can’t be disabled"
  );
});

async function toggleEnabled(editor, stylesheetToggle, panelWindow) {
  const changed = editor.once("property-change");

  info("Waiting for focus.");
  await SimpleTest.promiseFocus(panelWindow);

  info("Clicking on the toggle.");
  EventUtils.synthesizeMouseAtCenter(stylesheetToggle, {}, panelWindow);

  info("Waiting for stylesheet to be disabled.");
  let property = await changed;
  while (property !== "disabled") {
    info("Ignoring property-change for '" + property + "'.");
    property = await editor.once("property-change");
  }
}
