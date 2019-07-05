/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

// Verify that a styled input field element is showing proper font information
// in its font tab.
// Non-regression test for https://bugzilla.mozilla.org/show_bug.cgi?id=1435469
add_task(async function() {
  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;

  const onInspectorUpdated = inspector.once("fontinspector-updated");
  await selectNode(".input-field", inspector);

  info("Waiting for font editor to render");
  await onInspectorUpdated;

  const fontEls = getUsedFontsEls(viewDoc);
  ok(fontEls.length == 1, `Used fonts found for styled input element`);
  ok(
    fontEls[0].textContent == "Ostrich Sans Medium",
    `Proper font found: 'Ostrich Sans Medium' for styled input.`
  );
});
