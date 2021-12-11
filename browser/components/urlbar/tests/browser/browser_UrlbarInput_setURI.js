/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  // avoid prompting about phishing
  Services.prefs.setIntPref(phishyUserPassPref, 32);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(phishyUserPassPref);
  });

  nextTest();
}

const phishyUserPassPref = "network.http.phishy-userpass-length";

function nextTest() {
  let testCase = tests.shift();
  if (testCase) {
    testCase(function() {
      executeSoon(nextTest);
    });
  } else {
    executeSoon(finish);
  }
}

var tests = [
  function revert(next) {
    loadTabInWindow(window, function(tab) {
      gURLBar.handleRevert();
      is(
        gURLBar.value,
        "example.com",
        "URL bar had user/pass stripped after reverting"
      );
      gBrowser.removeTab(tab);
      next();
    });
  },
  function customize(next) {
    // Need to wait for delayedStartup for the customization part of the test,
    // since that's where BrowserToolboxCustomizeDone is set.
    BrowserTestUtils.openNewBrowserWindow().then(function(win) {
      loadTabInWindow(win, function() {
        openToolbarCustomizationUI(function() {
          closeToolbarCustomizationUI(function() {
            is(
              win.gURLBar.value,
              "example.com",
              "URL bar had user/pass stripped after customize"
            );
            win.close();
            next();
          }, win);
        }, win);
      });
    });
  },
  function pageloaderror(next) {
    loadTabInWindow(window, function(tab) {
      // Load a new URL and then immediately stop it, to simulate a page load
      // error.
      BrowserTestUtils.loadURI(tab.linkedBrowser, "http://test1.example.com");
      tab.linkedBrowser.stop();
      is(
        gURLBar.value,
        "example.com",
        "URL bar had user/pass stripped after load error"
      );
      gBrowser.removeTab(tab);
      next();
    });
  },
];

function loadTabInWindow(win, callback) {
  info("Loading tab");
  let url = "http://user:pass@example.com/";
  let tab = (win.gBrowser.selectedTab = BrowserTestUtils.addTab(
    win.gBrowser,
    url
  ));
  BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, url).then(() => {
    info("Tab loaded");
    is(
      win.gURLBar.value,
      "example.com",
      "URL bar had user/pass stripped initially"
    );
    callback(tab);
  }, true);
}

function openToolbarCustomizationUI(aCallback, aBrowserWin) {
  if (!aBrowserWin) {
    aBrowserWin = window;
  }

  aBrowserWin.gCustomizeMode.enter();

  aBrowserWin.gNavToolbox.addEventListener(
    "customizationready",
    function() {
      executeSoon(function() {
        aCallback(aBrowserWin);
      });
    },
    { once: true }
  );
}

function closeToolbarCustomizationUI(aCallback, aBrowserWin) {
  aBrowserWin.gNavToolbox.addEventListener(
    "aftercustomization",
    function() {
      executeSoon(aCallback);
    },
    { once: true }
  );

  aBrowserWin.gCustomizeMode.exit();
}
