/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that correct previews are shown if the text is edited after 'Show all'
// button is pressed.

const TEST_URI = URL_ROOT + "browser_fontinspector.html";

add_task(function* () {
  let { inspector, view } = yield openFontInspectorForURL(TEST_URI);
  let viewDoc = view.chromeDoc;

  info("Selecting a node that doesn't contain all document fonts.");
  yield selectNode(".normal-text", inspector);

  let normalTextNumPreviews =
    viewDoc.querySelectorAll("#all-fonts .font-preview").length;

  let onUpdated = inspector.once("fontinspector-updated");

  info("Clicking 'Select all' button.");
  viewDoc.getElementById("font-showall").click();

  info("Waiting for font-inspector to update.");
  yield onUpdated;

  let allFontsNumPreviews =
    viewDoc.querySelectorAll("#all-fonts .font-preview").length;

  // Sanity check. If this fails all fonts apply also to the .normal-text node
  // meaning we won't detect if preview editing causes the panel not to show all
  // fonts.
  isnot(allFontsNumPreviews, normalTextNumPreviews,
    "The .normal-text didn't show all fonts.");

  info("Editing the preview text.");
  yield updatePreviewText(view, "The quick brown");

  let numPreviews = viewDoc.querySelectorAll("#all-fonts .font-preview").length;
  is(numPreviews, allFontsNumPreviews,
    "All fonts are still shown after the preview text was edited.");
});
