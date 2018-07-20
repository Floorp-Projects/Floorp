/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Check that the warning message for no fonts found shows up when selecting a node
// that does not have any used fonts.
// Ensure that no used fonts are listed.

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

add_task(async function() {
  await pushPref("devtools.inspector.fonteditor.enabled", true);
  const { view, inspector } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;
  await selectNode(".empty", inspector);

  info("Test the warning message for no fonts found on empty element");
  const warning = viewDoc.querySelector("#font-editor .devtools-sidepanel-no-result");
  ok(warning, "The warning for no fonts found is shown for the empty element");

  info("Test that no fonts are listed for the empty element");
  const fontsEls = getUsedFontsEls(viewDoc);
  is(fontsEls.length, 0, "There are no used fonts listed");
});
