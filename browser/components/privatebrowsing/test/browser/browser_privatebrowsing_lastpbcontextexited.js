/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  // We need to open a new window for this so that its docshell would get destroyed
  // when clearing the PB mode flag.
  function runTest(aCloseWindow, aCallback) {
    let newWin = OpenBrowserWindow({private: true});
    SimpleTest.waitForFocus(function() {
      let expectedExiting = true;
      let expectedExited = false;
      let observerExiting = {
        observe: function(aSubject, aTopic, aData) {
          is(aTopic, "last-pb-context-exiting", "Correct topic should be dispatched (exiting)");
          is(expectedExiting, true, "notification not expected yet (exiting)");
          expectedExited = true;
          Services.obs.removeObserver(observerExiting, "last-pb-context-exiting");
        }
      };
      let observerExited = {
        observe: function(aSubject, aTopic, aData) {
          is(aTopic, "last-pb-context-exited", "Correct topic should be dispatched (exited)");
          is(expectedExited, true, "notification not expected yet (exited)");
          Services.obs.removeObserver(observerExited, "last-pb-context-exited");
          aCallback();
        }
      };
      Services.obs.addObserver(observerExiting, "last-pb-context-exiting", false);
      Services.obs.addObserver(observerExited, "last-pb-context-exited", false);
      expectedExiting = true;
      aCloseWindow(newWin);
      newWin = null;
      SpecialPowers.forceGC();
    }, newWin);
  }

  waitForExplicitFinish();

  runTest(function(newWin) {
      // Simulate pressing the window close button
      newWin.document.getElementById("cmd_closeWindow").doCommand();
    }, function () {
      runTest(function(newWin) {
          // Simulate closing the last tab
          newWin.document.getElementById("cmd_close").doCommand();
        }, finish);
    });
}
