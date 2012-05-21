/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  // We need to open a new window for this so that its docshell would get destroyed
  // when clearing the PB mode flag.
  let newWin = window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");
  waitForExplicitFinish();
  SimpleTest.waitForFocus(function() {
    let notificationCount = 0;
    let observer = {
      observe: function(aSubject, aTopic, aData) {
        is(aTopic, "last-pb-context-exited", "Correct topic should be dispatched");
        ++notificationCount;
      }
    };
    Services.obs.addObserver(observer, "last-pb-context-exited", false);
    newWin.gPrivateBrowsingUI.privateWindow = true;
    SimpleTest.is(notificationCount, 0, "last-pb-context-exited should not be fired yet");
    newWin.gPrivateBrowsingUI.privateWindow = false;
    newWin.close();
    newWin = null;
    window.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDOMWindowUtils)
          .garbageCollect(); // Make sure that the docshell is destroyed
    SimpleTest.is(notificationCount, 1, "last-pb-context-exited should be fired once");
    Services.obs.removeObserver(observer, "last-pb-context-exited", false);

    // cleanup
    gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
    finish();
  }, newWin);
}
