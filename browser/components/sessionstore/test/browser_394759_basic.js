/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "data:text/html;charset=utf-8,<input%20id=txt>" +
                 "<input%20type=checkbox%20id=chk>";

/**
 * This test ensures that closing a window is a reversible action. We will
 * close the the window, restore it and check that all data has been restored.
 * This includes window-specific data as well as form data for tabs.
 */
function test() {
  waitForExplicitFinish();

  let uniqueKey = "bug 394759";
  let uniqueValue = "unik" + Date.now();
  let uniqueText = "pi != " + Math.random();

  // Clear the list of closed windows.
  forgetClosedWindows();

  provideWindow(function onTestURLLoaded(newWin) {
    newWin.gBrowser.addTab().linkedBrowser.stop();

    // mark the window with some unique data to be restored later on
    ss.setWindowValue(newWin, uniqueKey, uniqueValue);
    let [txt, chk] = newWin.content.document.querySelectorAll("#txt, #chk");
    txt.value = uniqueText;

    let browser = newWin.gBrowser.selectedBrowser;
    setInputChecked(browser, {id: "chk", checked: true}).then(() => {
      newWin.close();

      // Now give it time to close
      executeSoon(function() {
        is(ss.getClosedWindowCount(), 1,
           "The closed window was added to Recently Closed Windows");
        let data = JSON.parse(ss.getClosedWindowData())[0];
        ok(data.title == TEST_URL && JSON.stringify(data).indexOf(uniqueText) > -1,
           "The closed window data was stored correctly");

        // reopen the closed window and ensure its integrity
        let newWin2 = ss.undoCloseWindow(0);

        ok(newWin2 instanceof ChromeWindow,
           "undoCloseWindow actually returned a window");
        is(ss.getClosedWindowCount(), 0,
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

          is(restoredTabs, expectedTabs, "correct number of tabs restored");
          newWin2.removeEventListener("SSTabRestored", sstabrestoredListener, true);

          is(newWin2.gBrowser.tabs.length, 2,
             "The window correctly restored 2 tabs");
          is(newWin2.gBrowser.currentURI.spec, TEST_URL,
             "The window correctly restored the URL");

          let [txt, chk] = newWin2.content.document.querySelectorAll("#txt, #chk");
          ok(txt.value == uniqueText && chk.checked,
             "The window correctly restored the form");
          is(ss.getWindowValue(newWin2, uniqueKey), uniqueValue,
             "The window correctly restored the data associated with it");

          // clean up
          newWin2.close();
          finish();
        }, true);
      });
    });
  }, TEST_URL);
}

function setInputChecked(browser, data) {
  return sendMessage(browser, "ss-test:setInputChecked", data);
}
