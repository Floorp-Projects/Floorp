/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the style editor does not store any
// content CSS files in the permanent cache when opened from PB mode.

let gUI;

function test() {
  waitForExplicitFinish();
  let windowsToClose = [];
  let testURI = 'http://' + TEST_HOST + '/browser/browser/devtools/styleeditor/test/test_private.html';

  function checkCache() {
    checkDiskCacheFor(TEST_HOST, function() {
      gUI = null;
      finish();
    });
  }

  function doTest(aWindow) {
    aWindow.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      aWindow.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
      cache.clear();
      openStyleEditorInWindow(aWindow, function(panel) {
        gUI = panel.UI;
        gUI.on("editor-added", onEditorAdded);
      });
    }, true);

    aWindow.gBrowser.selectedBrowser.loadURI(testURI);
  }

  function onEditorAdded(aEvent, aEditor) {
    aEditor.getSourceEditor().then(checkCache);
  }

  function testOnWindow(options, callback) {
    let win = OpenBrowserWindow(options);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      windowsToClose.push(win);
      executeSoon(function() callback(win));
    }, false);
  };

  registerCleanupFunction(function() {
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  testOnWindow({private: true}, function(win) {
    doTest(win);
  });
}
