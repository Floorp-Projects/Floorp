/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
