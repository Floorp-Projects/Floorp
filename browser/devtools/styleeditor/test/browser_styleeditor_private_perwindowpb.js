/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the style editor does not store any
// content CSS files in the permanent cache when opened from PB mode.

function test() {
  waitForExplicitFinish();
  let gUI;
  let testURI = 'http://' + TEST_HOST + '/browser/browser/devtools/styleeditor/test/test_private.html';

  info("Opening a new private window");
  let win = OpenBrowserWindow({private: true});
  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    executeSoon(startTest);
  }, false);

  function startTest() {
    win.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      win.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);

      info("Clearing the browser cache");
      cache.clear();

      info("Opening the style editor in the private window");
      openStyleEditorInWindow(win, function(panel) {
        gUI = panel.UI;
        gUI.on("editor-added", onEditorAdded);
      });
    }, true);

    info("Loading the test URL in the new private window");
    win.content.location = testURI;
  }

  function onEditorAdded(aEvent, aEditor) {
    info("The style editor is ready")
    aEditor.getSourceEditor().then(checkCache);
  }

  function checkCache() {
    checkDiskCacheFor(TEST_HOST, function() {
      gUI.off("editor-added", onEditorAdded);
      win.close();
      win = null;
      gUI = null;
      finish();
    });
  }
}
