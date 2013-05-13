/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks that closed private windows can't be restored

function test() {
  waitForExplicitFinish();

  // Purging the list of closed windows
  while(ss.getClosedWindowCount() > 0)
    ss.forgetClosedWindow(0);

  // Load a private window, then close it 
  // and verify it doesn't get remembered for restoring
  var win = OpenBrowserWindow({private: true});

  whenWindowLoaded(win, function onload() {
    info("The private window got loaded");
    win.addEventListener("SSWindowClosing", function onclosing() {
      win.removeEventListener("SSWindowClosing", onclosing, false);
      executeSoon(function () {
        is (ss.getClosedWindowCount(), 0,
            "The private window should not have been stored");
        finish();
      });
    }, false);
    win.close();
  });
}
