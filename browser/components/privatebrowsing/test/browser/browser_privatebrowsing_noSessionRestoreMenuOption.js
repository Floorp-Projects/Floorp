/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks that the Session Restore menu option is not enabled in private mode

function test() {
  waitForExplicitFinish();

  function testNoSessionRestoreMenuItem() { 
    let win = OpenBrowserWindow({private: true});
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      ok(true, "The second private window got loaded");
      let srCommand = win.document.getElementById("Browser:RestoreLastSession");
      ok(srCommand, "The Session Restore command should exist");
      is(PrivateBrowsingUtils.isWindowPrivate(win), true,
         "PrivateBrowsingUtils should report the correct per-window private browsing status");
      is(srCommand.hasAttribute("disabled"), true,
         "The Session Restore command should be disabled in private browsing mode");
      win.close();
      finish();
    }, false);
  }

  let win = OpenBrowserWindow({private: true});
  win.addEventListener("load", function onload() {
    win.removeEventListener("load", onload, false);
    ok(true, "The first private window got loaded");
    win.gBrowser.addTab("about:mozilla");
    win.close();
    testNoSessionRestoreMenuItem();
  }, false);
}
