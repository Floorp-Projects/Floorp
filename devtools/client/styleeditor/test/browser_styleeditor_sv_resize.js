/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that resizing the source editor container doesn't move the caret.

const TESTCASE_URI = TEST_BASE_HTTP + "simple.html";

const {Toolbox} = require("devtools/client/framework/toolbox");

add_task(async function() {
  const { toolbox, ui } = await openStyleEditorForURL(TESTCASE_URI);

  is(ui.editors.length, 2, "There are 2 style sheets initially");

  info("Changing toolbox host to a window.");
  await toolbox.switchHost(Toolbox.HostType.WINDOW);

  const editor = await ui.editors[0].getSourceEditor();
  const originalSourceEditor = editor.sourceEditor;

  const hostWindow = toolbox.win.parent;
  const originalWidth = hostWindow.outerWidth;
  const originalHeight = hostWindow.outerHeight;

  // to check the caret is preserved
  originalSourceEditor.setCursor(originalSourceEditor.getPosition(4));

  info("Resizing window.");
  hostWindow.resizeTo(120, 480);

  const sourceEditor = ui.editors[0].sourceEditor;
  is(sourceEditor, originalSourceEditor,
     "the editor still references the same Editor instance");

  is(sourceEditor.getOffset(sourceEditor.getCursor()), 4,
     "the caret position has been preserved");

  info("Restoring window to original size.");
  hostWindow.resizeTo(originalWidth, originalHeight);
});

registerCleanupFunction(() => {
  // Restore the host type for other tests.
  Services.prefs.clearUserPref("devtools.toolbox.host");
});
