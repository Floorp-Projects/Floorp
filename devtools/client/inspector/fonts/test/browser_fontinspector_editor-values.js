/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = URL_ROOT + "browser_fontinspector.html";

add_task(async function() {
  await pushPref("devtools.inspector.fonteditor.enabled", true);
  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;

  await testDiv(inspector, viewDoc);
  await testNestedSpan(inspector, viewDoc);
});

function getFontSize(viewDoc) {
  const number =
    viewDoc.querySelector("#font-editor .font-value-slider[name=font-size]").value;
  const units =
    viewDoc.querySelector(`#font-editor .font-value-slider[name=font-size]
    ~ .font-unit-select`).value;

  return number + units;
}

async function testDiv(inspector, viewDoc) {
  await selectNode("DIV", inspector);
  const fontSize = getFontSize(viewDoc);

  is(fontSize, "1em", "DIV should be have font-size of 1em");
}

async function testNestedSpan(inspector, viewDoc) {
  await selectNode(".nested-span", inspector);
  const fontSize = getFontSize(viewDoc);

  isnot(fontSize, "1em", "Nested span should not reflect parent's font size.");
  is(fontSize, "36px", "Nested span should have computed font-size of 36px");
}
