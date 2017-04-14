/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the gPrivateBrowsingUI object, the Private Browsing
// menu item and its XUL <command> element work correctly.

function test() {
  // initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let testURI = "about:blank";
  let pbMenuItem;
  let cmd;

  function doTest(aIsPrivateMode, aWindow, aCallback) {
    aWindow.gBrowser.selectedBrowser.addEventListener("load", function() {
      ok(aWindow.gPrivateBrowsingUI, "The gPrivateBrowsingUI object exists");

      pbMenuItem = aWindow.document.getElementById("menu_newPrivateWindow");
      ok(pbMenuItem, "The Private Browsing menu item exists");

      cmd = aWindow.document.getElementById("Tools:PrivateBrowsing");
      isnot(cmd, null, "XUL command object for the private browsing service exists");

      is(pbMenuItem.getAttribute("label"), "New Private Window",
        "The Private Browsing menu item should read \"New Private Window\"");
      is(PrivateBrowsingUtils.isWindowPrivate(aWindow), aIsPrivateMode,
        "PrivateBrowsingUtils should report the correct per-window private browsing status (privateBrowsing should be " +
        aIsPrivateMode + ")");

      aCallback();
    }, {capture: true, once: true});

    aWindow.gBrowser.selectedBrowser.loadURI(testURI);
  }

  function openPrivateBrowsingModeByUI(aWindow, aCallback) {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      aSubject.addEventListener("load", function() {
        Services.obs.removeObserver(observer, "domwindowopened");
          windowsToClose.push(aSubject);
          aCallback(aSubject);
      }, {once: true});
    }, "domwindowopened");

    cmd = aWindow.document.getElementById("Tools:PrivateBrowsing");
    var func = new Function("", cmd.getAttribute("oncommand"));
    func.call(cmd);
  }

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function(aWin) {
      windowsToClose.push(aWin);
      // execute should only be called when need, like when you are opening
      // web pages on the test. If calling executeSoon() is not necesary, then
      // call whenNewWindowLoaded() instead of testOnWindow() on your test.
      executeSoon(() => aCallback(aWin));
    });
  }

   // this function is called after calling finish() on the test.
  registerCleanupFunction(function() {
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
  });

  // test first when not on private mode
  testOnWindow({}, function(aWin) {
    doTest(false, aWin, function() {
      // then test when on private mode, opening a new private window from the
      // user interface.
      openPrivateBrowsingModeByUI(aWin, function(aPrivateWin) {
        doTest(true, aPrivateWin, finish);
      });
    });
  });
}
