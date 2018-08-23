/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Check that the font inspector has a section for "other fonts" and that section contains
// the fonts *not* used in the currently selected element.
// Check that it's collapsed by default, but can be expanded. That it does not contain
// the fonts listed in the previous section.
//
// NOTE: this test should be removed once Bug 1485324 is fixed.
// @see https://bugzilla.mozilla.org/show_bug.cgi?id=1485324

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

add_task(async function() {
  // Disable font editor because the "other fonts" section does not show up in that mode.
  await pushPref("devtools.inspector.fonteditor.enabled", false);
  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;

  const otherFontsAccordion = getFontsAccordion(viewDoc);
  ok(otherFontsAccordion, "There's an accordion in the panel");
  is(otherFontsAccordion.textContent, "Other fonts in page", "It has the right title");

  await expandAccordion(otherFontsAccordion);
  let allFontsEls = getAllFontsEls(viewDoc);

  is(allFontsEls.length, 1, "There is one font listed in the other fonts section");
  // On Linux Times New Roman does not exist, Liberation Serif is used instead
  const name = getName(allFontsEls[0]);
  ok(name === "Times New Roman" || name === "Liberation Serif",
     "The other font listed is the right one");

  info("Checking that fonts of the current element aren't listed");
  await selectNode(".bold-text", inspector);
  await expandFontsAccordion(viewDoc);
  allFontsEls = getAllFontsEls(viewDoc);

  for (const otherFontEl of allFontsEls) {
    ok(![...getUsedFontsEls_obsolete(viewDoc)].some(el =>
      getName(el) === getName(otherFontEl)),
       "Other font isn't listed in the main fonts section");
  }
});
