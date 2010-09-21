/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is sessionstore test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Paul O’Shannessy <paul@oshannessy.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function browserWindowsCount() {
  let count = 0;
  let e = Services.wm.getEnumerator("navigator:browser");
  while (e.hasMoreElements()) {
    if (!e.getNext().closed)
      ++count;
  }
  return count;
}

function test() {
  /** Test for Bug 597071 **/

  waitForExplicitFinish();

  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

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
      // make sure there are 2 windows open
      is(browserWindowsCount(), 2, "there should be 2 windows open currently");
      // make sure sessionstore sees this window
      let state = JSON.parse(ss.getBrowserState());
      is(state.windows.length, 2, "sessionstore knows about this window");

      newWin.close();
      newWin.addEventListener("unload", function(aEvent) {
        is(ss.getClosedWindowCount(), closedWindowCount + 1,
           "increased closed window count");
        is(browserWindowsCount(), 1, "there should be 1 window open currently");

        try {
          Services.prefs.clearUserPref("browser.sessionstore.max_windows_undo");
        } catch (e) {}
        ss.setBrowserState(currentState);
        executeSoon(finish);

      }, false);
    }, true);
  }, false);
}

