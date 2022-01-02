/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the style-editor initializes correctly for XUL windows.

"use strict";

waitForExplicitFinish();

const TEST_URL = TEST_BASE + "doc_xulpage.xhtml";

add_task(async function() {
  const tab = await addTab(TEST_URL);
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "styleeditor",
  });
  const panel = toolbox.getCurrentPanel();

  ok(
    panel,
    "The style-editor panel did initialize correctly for the XUL window"
  );
});
