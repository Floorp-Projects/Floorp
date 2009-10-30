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
 *   Marco Bonardo <mak77@bonardo.net>
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

  waitForExplicitFinish();

  // Set interval to a large time so state won't be written while we setup env.
  gPrefService.setIntPref("browser.sessionstore.interval", 100000);

  // Set up the browser in a blank state. Popup windows in previous tests result
  // in different states on different platforms.
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  let blankState = JSON.stringify({
    windows: [{
      tabs: [{ entries: [{ url: "about:blank" }] }],
      _closedTabs: []
    }],
    _closedWindows: []
  });
  ss.setBrowserState(blankState);

  // Wait for the sessionstore.js file to be written before going on.
  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver({observe: function(aSubject, aTopic, aData) {
    os.removeObserver(this, aTopic);
    info("sessionstore.js was written");
    if (gPrefService.prefHasUserValue("browser.sessionstore.interval"))
      gPrefService.clearUserPref("browser.sessionstore.interval");

    executeSoon(continue_test);
  }}, "sessionstore-state-write-complete", false);

  // Remove the sessionstore.js file before setting the interval to 0
  let profilePath = Cc["@mozilla.org/file/directory_service;1"].
                    getService(Ci.nsIProperties).
                    get("ProfD", Ci.nsIFile);
  let sessionStoreJS = profilePath.clone();
  sessionStoreJS.append("sessionstore.js");
  if (sessionStoreJS.exists())
    sessionStoreJS.remove(false);
  ok(sessionStoreJS.exists() == false, "sessionstore.js was removed");

  // Make sure that sessionstore.js can be forced to be created by setting
  // the interval pref to 0.
  gPrefService.setIntPref("browser.sessionstore.interval", 0);
}

function continue_test() {
  let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);

  let closedWindowCount = ss.getClosedWindowCount();
  is(closedWindowCount, 0, "Correctly set window count");

  // Prevent VM timers issues, cache now and increment it manually.
  let now = Date.now();
  const TESTS = [
    { url: "about:config",
      key: "bug 394759 Non-PB",
      value: "uniq" + (++now) },
    { url: "about:mozilla",
      key: "bug 394759 PB",
      value: "uniq" + (++now) },
  ];

  let loadWasCalled = false;
  function openWindowAndTest(aTestIndex, aRunNextTestInPBMode) {
    info("Opening new window");
    let windowObserver = {
      observe: function(aSubject, aTopic, aData) {
        if (aTopic === "domwindowopened") {
          info("New window has been opened");
          let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
          win.addEventListener("load", function onLoad(event) {
            win.removeEventListener("load", onLoad, false);
            info("New window has been loaded");
            win.gBrowser.addEventListener("load", function(aEvent) {
              win.gBrowser.removeEventListener("load", arguments.callee, true);
              info("New window browser has been loaded");
              loadWasCalled = true;
              executeSoon(function() {
                // Add a tab.
                win.gBrowser.addTab();

                executeSoon(function() {
                  // Mark the window with some unique data to be restored later on.
                  ss.setWindowValue(win, TESTS[aTestIndex].key, TESTS[aTestIndex].value);

                  win.close();

                  // Ensure that we incremented # of close windows.
                  is(ss.getClosedWindowCount(), closedWindowCount + 1,
                     "The closed window was added to the list");

                  // Ensure we added window to undo list.
                  let data = JSON.parse(ss.getClosedWindowData())[0];
                  ok(data.toSource().indexOf(TESTS[aTestIndex].value) > -1,
                     "The closed window data was stored correctly");

                  if (aRunNextTestInPBMode) {
                    // Enter private browsing mode.
                    pb.privateBrowsingEnabled = true;
                    ok(pb.privateBrowsingEnabled, "private browsing enabled");

                    // Ensure that we have 0 undo windows when entering PB.
                    is(ss.getClosedWindowCount(), 0,
                       "Recently Closed Windows are removed when entering Private Browsing");
                    is(ss.getClosedWindowData(), "[]",
                       "Recently Closed Windows data is cleared when entering Private Browsing");
                  }
                  else {
                    // Exit private browsing mode.
                    pb.privateBrowsingEnabled = false;
                    ok(!pb.privateBrowsingEnabled, "private browsing disabled");

                    // Ensure that we still have the closed windows from before.
                    is(ss.getClosedWindowCount(), closedWindowCount + 1,
                       "The correct number of recently closed windows were restored " +
                       "when exiting PB mode");

                    let data = JSON.parse(ss.getClosedWindowData())[0];
                    ok(data.toSource().indexOf(TESTS[aTestIndex - 1].value) > -1,
                       "The data associated with the recently closed window was " +
                       "restored when exiting PB mode");
                  }

                  if (aTestIndex == TESTS.length - 1)
                    finish();
                  else {
                    // Run next test.
                    openWindowAndTest(aTestIndex + 1, !aRunNextTestInPBMode);
                  }
                });
              });
            }, true);
          }, false);
        }
        else if (aTopic === "domwindowclosed") {
          info("Window closed");
          ww.unregisterNotification(this);
          if (!loadWasCalled) {
            ok(false, "Window was closed before load could fire!");
            finish();
          }
        }
      }
    };
    ww.registerNotification(windowObserver);
    // Open a window.
    openDialog(location, "_blank", "chrome,all,dialog=no", TESTS[aTestIndex].url);
  }

  openWindowAndTest(0, true);
}
