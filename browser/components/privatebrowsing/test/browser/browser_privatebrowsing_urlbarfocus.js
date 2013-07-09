/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the URL bar is focused when entering the private window.

function test() {
  waitForExplicitFinish();

  const TEST_URL = "data:text/plain,test";

  function checkUrlbarFocus(aWin, aIsPrivate, aCallback) {
    let urlbar = aWin.gURLBar;
    if (aIsPrivate) {
      is(aWin.document.commandDispatcher.focusedElement, urlbar.inputField,
         "URL Bar should be focused inside the private window");
      is(urlbar.value, "",
         "URL Bar should be empty inside the private window");
    } else {
      isnot(aWin.document.commandDispatcher.focusedElement, urlbar.inputField,
            "URL Bar should not be focused after opening window");
      isnot(urlbar.value, "",
            "URL Bar should not be empty after opening window");
    }
    aCallback();
  }

  let windowsToClose = [];
  function testOnWindow(aPrivate, aCallback) {
    whenNewWindowLoaded({private: aPrivate}, function(win) {
      windowsToClose.push(win);
      executeSoon(function() aCallback(win));
    });
  }

  function doneWithTests() {
    windowsToClose.forEach(function(win) {
      win.close();
    });
    finish();
  }

  function whenLoadTab(aPrivate, aCallback) {
    testOnWindow(aPrivate, function(win) {
      if (!aPrivate) {
        let browser = win.gBrowser.selectedBrowser;
        browser.addEventListener("load", function() {
          browser.removeEventListener("load", arguments.callee, true);
          aCallback(win);
        }, true);
        browser.focus();
        browser.loadURI(TEST_URL);
      } else {
        aCallback(win);
      }
    });
  }

  whenLoadTab(false, function(win) {
    checkUrlbarFocus(win, false, function() {
      whenLoadTab(true, function(win) {
        checkUrlbarFocus(win, true, function() {
          whenLoadTab(false, function(win) {
            checkUrlbarFocus(win, false, doneWithTests);
          });
        });
      });
    });
  });
}
