/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

// Test that unused font families show up in the font editor.
add_task(async function() {
  await pushPref("devtools.inspector.fonteditor.enabled", true);
  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;

  await testFamiliesUnused(inspector, viewDoc);
  await testZeroFamiliesUnused(inspector, viewDoc);
});

function getUnusedFontFamilies(viewDoc) {
  return [...viewDoc.querySelectorAll("#font-editor .font-family-unused")]
    .map(el => el.textContent);
}

async function testFamiliesUnused(inspector, viewDoc) {
  await selectNode("div", inspector);

  const unused = getUnusedFontFamilies(viewDoc);
  is(unused.length, 2, "Two font families were not used");
  is(unused[0], "Missing Family", "First unused family is correct");
  is(unused[1], "sans-serif", "Second unused family is correct");
}

async function testZeroFamiliesUnused(inspector, viewDoc) {
  await selectNode(".normal-text", inspector);

  const unused = getUnusedFontFamilies(viewDoc);
  const header = viewDoc.querySelector("#font-editor .font-family-unused-header");
  is(unused.length, 0, "All font families were used");
  is(header, null, "Container for unused font families was not rendered");
}
