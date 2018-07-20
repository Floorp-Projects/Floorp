/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* global getPropertyValue waitFor */

"use strict";

// Test that changes to font-size units converts the value to the destination unit.
// Check that converted values are applied to the inline style of the selected element.

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

add_task(async function() {
  await pushPref("devtools.inspector.fonteditor.enabled", true);
  const { inspector, view, tab, testActor } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;
  const property = "font-size";
  const selector = "div";
  const UNITS = {
    "px": "36px",
    "%": "100%",
    "em": "1em",
  };

  await selectNode(selector, inspector);

  info("Check that font editor shows font-size value in original units");
  let fontSize = getPropertyValue(viewDoc, property);
  is(fontSize.unit, "em", "Original unit for font size is em");
  is(fontSize.value + fontSize.unit, "1em", "Original font size is 1em");

  let prevValue = fontSize.value;
  let prevUnit = fontSize.unit;

  for (const unit in UNITS) {
    const value = UNITS[unit];
    const onEditorUpdated = inspector.once("fonteditor-updated");

    info(`Convert font-size from ${prevValue}${prevUnit} to ${unit}`);
    await view.onPropertyChange(property, prevValue, prevUnit, unit);
    info("Waiting for font editor to re-render");
    await onEditorUpdated;
    info(`Waiting for font-size unit dropdown to re-render with selected value: ${unit}`);
    await waitFor(() => {
      const sel = `#font-editor .font-value-slider[name=${property}] ~ .font-unit-select`;
      return viewDoc.querySelector(sel).value === unit;
    });
    info("Waiting for testactor reflow");
    await testActor.reflow();

    info(`Check that font editor font-size value is converted to ${unit}`);
    fontSize = getPropertyValue(viewDoc, property);
    is(fontSize.unit, unit, `Font size unit is converted to ${unit}`);
    is(fontSize.value + fontSize.unit, value, `Font size in font editor is ${value}`);

    info(`Check that inline style font-size value is converted to ${unit}`);
    const inlineStyleValue = await getInlineStyleValue(tab, selector, property);
    is(inlineStyleValue, value, `Font size on inline style is ${value}`);

    // Store current unit and value to use in conversion on the next iteration.
    prevUnit = fontSize.unit;
    prevValue = fontSize.value;
  }
});

async function getInlineStyleValue(tab, selector, property) {
  return ContentTask.spawn(tab.linkedBrowser, { selector, property }, function(args) {
    const el = content.document.querySelector(args.selector);
    return el && el.style[args.property];
  });
}
