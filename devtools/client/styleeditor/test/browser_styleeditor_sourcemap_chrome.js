/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = URL_ROOT_SSL + "doc_sourcemap_chrome.html";
const CHROME_TEST_URI = CHROME_URL_ROOT + "doc_sourcemap_chrome.html";
const GENERATED_NAME = "sourcemaps_chrome.css";
const ORIGINAL_NAME = "sourcemaps.scss";

/**
 * Test that a sourcemap served by a chrome URL for a http document will not be resolved.
 */
add_task(async function () {
  const { ui } = await openStyleEditorForURL(TEST_URI);
  ok(
    findStylesheetByName(ui, GENERATED_NAME),
    "Sourcemap not resolved: generated source is listed"
  );
  ok(
    !findStylesheetByName(ui, ORIGINAL_NAME),
    "Sourcemap not resolved: original source is not listed"
  );
});

/**
 * Test that a sourcemap served by a chrome URL for a chrome document is resolved.
 */
add_task(async function () {
  const { ui } = await openStyleEditorForURL(CHROME_TEST_URI);
  ok(
    findStylesheetByName(ui, ORIGINAL_NAME),
    "Sourcemap resolved: original source is listed"
  );
  ok(
    !findStylesheetByName(ui, GENERATED_NAME),
    "Sourcemap resolved: generated source is not listed"
  );
});

function findStylesheetByName(ui, name) {
  return ui.editors.some(
    editor =>
      editor.summary
        .querySelector(".stylesheet-name > label")
        .getAttribute("value") === name
  );
}
