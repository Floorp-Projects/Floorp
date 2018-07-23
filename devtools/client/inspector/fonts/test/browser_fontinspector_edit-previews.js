/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that previews change when the preview text changes. It doesn't check the
// exact preview images because they are drawn on a canvas causing them to vary
// between systems, platforms and software versions.

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

add_task(async function() {
  await pushPref("devtools.inspector.fonteditor.enabled", true);
  const { view, inspector } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;
  await selectNode("div", inspector);
  await expandOtherFontsAccordion(viewDoc);

  const previews = viewDoc.querySelectorAll("#font-container .font-preview");
  const initialPreviews = [...previews].map(p => p.src);

  info("Typing 'Abc' to check that the reference previews are correct.");
  await updatePreviewText(view, "Abc");
  checkPreviewImages(viewDoc, initialPreviews, true);

  info("Typing something else to the preview box.");
  await updatePreviewText(view, "The quick brown");
  checkPreviewImages(viewDoc, initialPreviews, false);

  info("Blanking the input to restore default previews.");
  await updatePreviewText(view, "");
  checkPreviewImages(viewDoc, initialPreviews, true);
});

/**
 * Compares the previous preview image URIs to the current URIs.
 *
 * @param {Document} viewDoc
 *        The FontInspector document.
 * @param {Array[String]} originalURIs
 *        An array of URIs to compare with the current URIs.
 * @param {Boolean} assertIdentical
 *        If true, this method asserts that the previous and current URIs are
 *        identical. If false, this method asserts that the previous and current
 *        URI's are different.
 */
function checkPreviewImages(viewDoc, originalURIs, assertIdentical) {
  const previews = viewDoc.querySelectorAll("#font-container .font-preview");
  const newURIs = [...previews].map(p => p.src);

  is(newURIs.length, originalURIs.length,
    "The number of previews has not changed.");

  for (let i = 0; i < newURIs.length; ++i) {
    if (assertIdentical) {
      is(newURIs[i], originalURIs[i],
        `The preview image at index ${i} has stayed the same.`);
    } else {
      isnot(newURIs[i], originalURIs[i],
        `The preview image at index ${i} has changed.`);
    }
  }
}
