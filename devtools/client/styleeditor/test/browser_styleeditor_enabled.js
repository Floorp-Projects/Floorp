/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that style sheets can be disabled and enabled.

// https rather than chrome to improve coverage
const SIMPLE_URI = TEST_BASE_HTTPS + "simple.html";
const LONGNAME_URI = TEST_BASE_HTTPS + "longname.html";

add_task(async function () {
  const { panel, ui } = await openStyleEditorForURL(SIMPLE_URI);
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

// Check that stylesheets with long names do not prevent the toggle button
// from being visible.
add_task(async function testLongNameStylesheet() {
  const { ui } = await openStyleEditorForURL(LONGNAME_URI);

  is(ui.editors.length, 2, "Expected 2 stylesheet editors");

  // Test that the first editor, which should have a stylesheet with a short
  // name.
  let editor = ui.editors[0];
  let stylesheetToggle = editor.summary.querySelector(".stylesheet-toggle");
  is(editor.friendlyName, "simple.css");
  ok(stylesheetToggle, "stylesheet toggle button exists");
  ok(stylesheetToggle.getBoundingClientRect().width > 0);
  ok(stylesheetToggle.getBoundingClientRect().height > 0);

  const expectedWidth = stylesheetToggle.getBoundingClientRect().width;
  const expectedHeight = stylesheetToggle.getBoundingClientRect().height;

  // Test that the second editor, which should have a stylesheet with a long
  // name.
  editor = ui.editors[1];
  stylesheetToggle = editor.summary.querySelector(".stylesheet-toggle");
  is(editor.friendlyName, "veryveryverylongnamethatcanbreakthestyleeditor.css");
  ok(stylesheetToggle, "stylesheet toggle button exists");
  is(stylesheetToggle.getBoundingClientRect().width, expectedWidth);
  is(stylesheetToggle.getBoundingClientRect().height, expectedHeight);
});

add_task(async function testSystemStylesheet() {
  const { ui } = await openStyleEditorForURL("about:support");

  const aboutSupportEditor = ui.editors.find(
    editor => editor.friendlyName === "aboutSupport.css"
  );
  ok(!!aboutSupportEditor, "Found the editor for aboutSupport.css");
  const aboutSupportToggle =
    aboutSupportEditor.summary.querySelector(".stylesheet-toggle");
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
  // For some unexplained reason, this is updated asynchronously
  await waitFor(
    () =>
      formsToggle.getAttribute("tooltiptext") ==
      "System style sheets can’t be disabled"
  );
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
