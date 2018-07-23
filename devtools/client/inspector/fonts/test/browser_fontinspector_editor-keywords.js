/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* global getPropertyValue */

"use strict";

// Test that keyword values for font properties don't show up in the font editor,
// but their computed style values show up instead.

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

add_task(async function() {
  await pushPref("devtools.inspector.fonteditor.enabled", true);
  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;

  await testKeywordValues(inspector, viewDoc);
});

async function testKeywordValues(inspector, viewDoc) {
  await selectNode(".bold-text", inspector);

  info("Check font-weight shows its computed style instead of the bold keyword value.");
  const fontWeight = getPropertyValue(viewDoc, "font-weight");
  isnot(fontWeight.value, "bold", "Font weight is not shown as keyword");
  is(fontWeight.value, "700", "Font weight is shown as computed style");

  info("Check font-size shows its computed style instead of the inherit keyword value.");
  const fontSize = getPropertyValue(viewDoc, "font-size");
  isnot(fontSize.unit, "inherit", "Font size unit is not shown as keyword");
  is(fontSize.unit, "px", "Font size unit is shown as computed style");
  is(fontSize.value + fontSize.unit, "36px", "Font size is read as computed style");
}
