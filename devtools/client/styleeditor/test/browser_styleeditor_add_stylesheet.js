/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that a newly-added style sheet shows up in the style editor.

const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

add_task(async function() {
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);

  is(ui.editors.length, 2, "Two sheets present after load.");

  // We have to wait for the length to change, because we might still
  // be seeing events from the initial open.
  const added = new Promise(resolve => {
    const handler = () => {
      if (ui.editors.length === 3) {
        ui.off("editor-added", handler);
        resolve();
      }
    };
    ui.on("editor-added", handler);
  });

  info("Adding a style sheet");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    const document = content.document;
    const style = document.createElement("style");
    style.appendChild(document.createTextNode("div { background: #f06; }"));
    document.head.appendChild(style);
  });
  await added;

  is(ui.editors.length, 3, "Three sheets present after new style sheet");
});
