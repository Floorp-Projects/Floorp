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
 * Simon Bünzli <zeniko@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

function provideWindow(aCallback, aURL, aFeatures) {
  function callback() {
    executeSoon(function () {
      aCallback(win);
    });
  }

  let win = openDialog(getBrowserURL(), "", aFeatures || "chrome,all,dialog=no", aURL);

  whenWindowLoaded(win, function () {
    if (!aURL) {
      callback();
      return;
    }
    win.gBrowser.selectedBrowser.addEventListener("load", function() {
      win.gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
      callback();
    }, true);
  });
}

function whenWindowLoaded(aWin, aCallback) {
  aWin.addEventListener("load", function () {
    aWin.removeEventListener("load", arguments.callee, false);
    executeSoon(function () {
      aCallback(aWin);
    });
  }, false);
}

function test() {
  waitForExplicitFinish();

  let testURL = "about:config";
  let uniqueKey = "bug 394759";
  let uniqueValue = "unik" + Date.now();
  let uniqueText = "pi != " + Math.random();

  // Be consistent: let the page actually display, as we are "interacting" with it.
  Services.prefs.setBoolPref("general.warnOnAboutConfig", false);

  // make sure that the next closed window will increase getClosedWindowCount
  let max_windows_undo = Services.prefs.getIntPref("browser.sessionstore.max_windows_undo");
  Services.prefs.setIntPref("browser.sessionstore.max_windows_undo", max_windows_undo + 1);
  let closedWindowCount = ss.getClosedWindowCount();

  provideWindow(function onTestURLLoaded(newWin) {
    newWin.gBrowser.addTab().linkedBrowser.stop();

    // mark the window with some unique data to be restored later on
    ss.setWindowValue(newWin, uniqueKey, uniqueValue);
    let textbox = newWin.content.document.getElementById("textbox");
    textbox.value = uniqueText;

    newWin.close();

    is(ss.getClosedWindowCount(), closedWindowCount + 1,
       "The closed window was added to Recently Closed Windows");
    let data = JSON.parse(ss.getClosedWindowData())[0];
    ok(data.title == testURL && JSON.stringify(data).indexOf(uniqueText) > -1,
       "The closed window data was stored correctly");

    // reopen the closed window and ensure its integrity
    let newWin2 = ss.undoCloseWindow(0);

    ok(newWin2 instanceof ChromeWindow,
       "undoCloseWindow actually returned a window");
    is(ss.getClosedWindowCount(), closedWindowCount,
       "The reopened window was removed from Recently Closed Windows");

    // SSTabRestored will fire more than once, so we need to make sure we count them
    let restoredTabs = 0;
    let expectedTabs = data.tabs.length;
    newWin2.addEventListener("SSTabRestored", function sstabrestoredListener(aEvent) {
      ++restoredTabs;
      info("Restored tab " + restoredTabs + "/" + expectedTabs);
      if (restoredTabs < expectedTabs) {
        return;
      }

      newWin2.removeEventListener("SSTabRestored", sstabrestoredListener, true);

      is(newWin2.gBrowser.tabs.length, 2,
         "The window correctly restored 2 tabs");
      is(newWin2.gBrowser.currentURI.spec, testURL,
         "The window correctly restored the URL");

      let textbox = newWin2.content.document.getElementById("textbox");
      is(textbox.value, uniqueText,
         "The window correctly restored the form");
      is(ss.getWindowValue(newWin2, uniqueKey), uniqueValue,
         "The window correctly restored the data associated with it");

      // clean up
      newWin2.close();
      Services.prefs.clearUserPref("browser.sessionstore.max_windows_undo");
      Services.prefs.clearUserPref("general.warnOnAboutConfig");
      finish();
    }, true);
  }, testURL);
}
