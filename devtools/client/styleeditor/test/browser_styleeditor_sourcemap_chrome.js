/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = URL_ROOT_SSL + "doc_sourcemap_chrome.html";
const CSS_NAME = "sourcemaps_chrome.css";

/**
 * Test that a sourcemap served by a chrome URL will not be resolved
 */
add_task(async function() {
  const { ui } = await openStyleEditorForURL(TEST_URI);
  const editor = ui.editors[0];

  // The CSS file contains a link to a sourcemap called which should map this
  // CSS file to "sourcemaps.scss". If the CSS is still listed as CSS_NAME, it
  // means the sourcemapped file was not resolved.
  is(getStylesheetNameFor(editor), CSS_NAME, "The sourcemap was not resolved");
});

function getStylesheetNameFor(editor) {
  return editor.summary
    .querySelector(".stylesheet-name > label")
    .getAttribute("value");
}
