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
 * Aaron Train <aaron.train@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *   Paul O’Shannessy <paul@oshannessy.com>
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

function test() {
  /** Private Browsing Test for Bug 394759 **/

  // test setup
  waitForExplicitFinish();

  // private browsing service
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let profilePath = Cc["@mozilla.org/file/directory_service;1"].
                    getService(Ci.nsIProperties).
                    get("ProfD", Ci.nsIFile);

  // sessionstore service
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  // Remove the sessionstore.js file before setting the interval to 0
  let sessionStoreJS = profilePath.clone();
  sessionStoreJS.append("sessionstore.js");
  if (sessionStoreJS.exists())
    sessionStoreJS.remove(false);
  ok(sessionStoreJS.exists() == false, "sessionstore.js was removed");
  // Make sure that sessionstore.js can be forced to be created by setting
  // the interval pref to 0
  gPrefService.setIntPref("browser.sessionstore.interval", 0);
  // sessionstore.js should be re-created at this point
  sessionStoreJS = profilePath.clone();
  sessionStoreJS.append("sessionstore.js");

  // Set up the browser in a blank state. Popup windows in previous tests result
  // in different states on different platforms.
  let blankState = JSON.stringify({
    windows: [{
      tabs: [{ entries: [{ url: "about:blank" }] }],
      _closedTabs: []
    }],
    _closedWindows: []
  });
  ss.setBrowserState(blankState);

  let closedWindowCount = ss.getClosedWindowCount();
  is(closedWindowCount, 0, "Correctly set window count");

  let testURL_A = "about:config";
  let testURL_B = "about:mozilla";

  let uniqueKey_A = "bug 394759 Non-PB";
  let uniqueValue_A = "unik" + Date.now();
  let uniqueKey_B = "bug 394759 PB";
  let uniqueValue_B = "uniq" + Date.now();


  // Open a window
  let newWin = openDialog(location, "_blank", "chrome,all,dialog=no", testURL_A);
  newWin.addEventListener("load", function(aEvent) {
    newWin.removeEventListener("load", arguments.callee, false);
    info("New window has been opened");
    newWin.gBrowser.addEventListener("pageshow", function(aEvent) {
      newWin.gBrowser.removeEventListener("pageshow", arguments.callee, true);
      info("Content has been loaded");
      executeSoon(function() {
        newWin.gBrowser.addTab();
        executeSoon(function() {
          // mark the window with some unique data to be restored later on
          ss.setWindowValue(newWin, uniqueKey_A, uniqueValue_A);

          newWin.close();

          // ensure that we incremented # of close windows
          is(ss.getClosedWindowCount(), closedWindowCount + 1,
             "The closed window was added to the list");

          // ensure we added window to undo list
          let data = JSON.parse(ss.getClosedWindowData())[0];
          ok(data.toSource().indexOf(uniqueValue_A) > -1,
             "The closed window data was stored correctly");

          // enter private browsing mode
          pb.privateBrowsingEnabled = true;
          ok(pb.privateBrowsingEnabled, "private browsing enabled");

          // ensure that we have 0 undo windows when entering PB
          is(ss.getClosedWindowCount(), 0,
             "Recently Closed Windows are removed when entering Private Browsing");
          is(ss.getClosedWindowData(), "[]",
             "Recently Closed Windows data is cleared when entering Private Browsing");

          // open another window in PB
          let pbWin = openDialog(location, "_blank", "chrome,all,dialog=no", testURL_B);
          pbWin.addEventListener("load", function(aEvent) {
            pbWin.removeEventListener("load", arguments.callee, false);
            pbWin.gBrowser.addEventListener("pageshow", function(aEvent) {
              pbWin.gBrowser.removeEventListener("pageshow", arguments.callee, true);

              executeSoon(function() {
                // Add another tab, though it's not strictly needed
                pbWin.gBrowser.addTab();
                executeSoon(function() {
                  // mark the window with some unique data to be restored later on
                  ss.setWindowValue(pbWin, uniqueKey_B, uniqueValue_B);

                  pbWin.close();

                  // ensure we added window to undo list
                  let data = JSON.parse(ss.getClosedWindowData())[0];
                  ok(data.toSource().indexOf(uniqueValue_B) > -1,
                     "The closed window data was stored correctly in PB mode");

                  // exit private browsing mode
                  pb.privateBrowsingEnabled = false;
                  ok(!pb.privateBrowsingEnabled, "private browsing disabled");

                  // ensure that we still have the closed windows from before
                  is(ss.getClosedWindowCount(), closedWindowCount + 1,
                     "The correct number of recently closed windows were restored " +
                     "when exiting PB mode");

                  let data = JSON.parse(ss.getClosedWindowData())[0];
                  ok(data.toSource().indexOf(uniqueValue_A) > -1,
                     "The data associated with the recently closed window was " +
                     "restored when exiting PB mode");

                  // cleanup
                  gPrefService.clearUserPref("browser.sessionstore.interval");
                  finish();
                });
              });
            }, true);
          }, false);
        });
      });
    }, true);
  }, false);
}
