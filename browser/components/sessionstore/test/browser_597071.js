/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 597071 **/

  waitForExplicitFinish();

  // set the pref to 1 greater than it currently is so we have room for an extra
  // closed window
  let closedWindowCount = ss.getClosedWindowCount();
  Services.prefs.setIntPref("browser.sessionstore.max_windows_undo",
                            closedWindowCount + 1);

  let currentState = ss.getBrowserState();
  let popupState = { windows:[
    { tabs:[ {entries:[] }], isPopup: true, hidden: "toolbar" }
  ] };

  // set this window to be a popup.
  ss.setWindowState(window, JSON.stringify(popupState), true);

  // open a new non-popup window
  let newWin = openDialog(location, "", "chrome,all,dialog=no", "http://example.com");
  newWin.addEventListener("load", function(aEvent) {
    newWin.removeEventListener("load", arguments.callee, false);

    newWin.gBrowser.addEventListener("load", function(aEvent) {
      newWin.gBrowser.removeEventListener("load", arguments.callee, true);

      newWin.gBrowser.addTab().linkedBrowser.stop();

      // make sure sessionstore sees this window
      let state = JSON.parse(ss.getBrowserState());
      is(state.windows.length, 2, "sessionstore knows about this window");

      newWin.close();
      newWin.addEventListener("unload", function(aEvent) {
        newWin.removeEventListener("unload", arguments.callee, false);

        is(ss.getClosedWindowCount(), closedWindowCount + 1,
           "increased closed window count");

        Services.prefs.clearUserPref("browser.sessionstore.max_windows_undo");
        ss.setBrowserState(currentState);
        executeSoon(finish);

      }, false);
    }, true);
  }, false);
}

