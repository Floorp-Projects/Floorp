/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  // We need to open a new window for this so that its docshell would get destroyed
  // when clearing the PB mode flag.
  let newWin = window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");
  waitForExplicitFinish();
  SimpleTest.waitForFocus(function() {
    let expected = false;
    let observer = {
      observe: function(aSubject, aTopic, aData) {
        is(aTopic, "last-pb-context-exited", "Correct topic should be dispatched");
        is(expected, true, "notification not expected yet");
        Services.obs.removeObserver(observer, "last-pb-context-exited", false);
        gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
        finish();
      }
    };
    Services.obs.addObserver(observer, "last-pb-context-exited", false);
    newWin.gPrivateBrowsingUI.privateWindow = true;
    expected = true;
    newWin.close(); // this will cause the docshells to leave PB mode
    newWin = null;
    SpecialPowers.forceGC();
  }, newWin);
}
