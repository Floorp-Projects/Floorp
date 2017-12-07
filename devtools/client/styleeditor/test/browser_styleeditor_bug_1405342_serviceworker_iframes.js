/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that sheets inside cross origin iframes, served from a service worker
// are correctly fetched via the service worker in the stylesheet editor.

add_task(async function () {
  const TEST_URL = "https://test1.example.com/browser/devtools/client/styleeditor/test/bug_1405342_serviceworker_iframes.html";
  let { ui } = await openStyleEditorForURL(TEST_URL);

  if (ui.editors.length != 1) {
    info("Stylesheet isn't available immediately, waiting for it");
    await ui.once("editor-added");
  }
  is(ui.editors.length, 1, "Got the iframe stylesheet");

  await ui.selectStyleSheet(ui.editors[0].styleSheet);
  let editor = await ui.editors[0].getSourceEditor();
  let text = editor.sourceEditor.getText();
  is(text, "* { color: green; }",
    "stylesheet content is the one served by the service worker");
});
