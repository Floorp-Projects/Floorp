/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that privatebrowsingmode attribute of the window is correctly
// adjusted based on whether the window is a private window.

var windowsToClose = [];
function testOnWindow(options, callback) {
  var win = OpenBrowserWindow(options);
  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    windowsToClose.push(win);
    executeSoon(function() callback(win));
  }, false);
}

registerCleanupFunction(function() {
  windowsToClose.forEach(function(win) {
    win.close();
  });
});

function test() {
  // initialization
  waitForExplicitFinish();

  ok(!document.documentElement.hasAttribute("privatebrowsingmode"),
    "privatebrowsingmode should not be present in normal mode");

  // open a private window
  testOnWindow({private: true}, function(win) {
    is(win.document.documentElement.getAttribute("privatebrowsingmode"), "temporary",
      "privatebrowsingmode should be \"temporary\" inside the private browsing mode");

    finish();
  });
}
