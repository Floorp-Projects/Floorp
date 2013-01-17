/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that private browsing mode disables the "remember"
// option in the cookie accept dialog.

function test() {
  // initialization
  const TEST_URL = "http://mochi.test:8888/browser/browser/components/" +
                   "privatebrowsing/test/browser/perwindow/" + 
                   "browser_privatebrowsing_cookieacceptdialog.html";
  let cp = Cc["@mozilla.org/embedcomp/cookieprompt-service;1"].
           getService(Ci.nsICookiePromptService);

  waitForExplicitFinish();

  function checkRememberOption(expectedDisabled, aWindow, callback) {
    function observer(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened")
        return;

      Services.ww.unregisterNotification(observer);
      let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
      win.addEventListener("load", function onLoad(event) {
        win.removeEventListener("load", onLoad, false);

        executeSoon(function () {
          let doc = win.document;
          let remember = doc.getElementById("persistDomainAcceptance");
          ok(remember, "The remember checkbox should exist");

          if (expectedDisabled)
            is(remember.getAttribute("disabled"), "true",
               "The checkbox should be disabled");
          else
            ok(!remember.hasAttribute("disabled"),
               "The checkbox should not be disabled");

          win.close();
          executeSoon(callback);
        });
      }, false);
    }
    Services.ww.registerNotification(observer);

    let remember = {};
    const time = (new Date("Jan 1, 2030")).getTime() / 1000;
    let cookie = {
      name: "foo",
      value: "bar",
      isDomain: true,
      host: "mozilla.org",
      path: "/baz",
      isSecure: false,
      expires: time,
      status: 0,
      policy: 0,
      isSession: false,
      expiry: time,
      isHttpOnly: true,
      QueryInterface: function(iid) {
        const validIIDs = [Ci.nsISupports, Ci.nsICookie, Ci.nsICookie2];
        for (var i = 0; i < validIIDs.length; ++i)
          if (iid == validIIDs[i])
            return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
      }
    };
    cp.cookieDialog(aWindow, cookie, "mozilla.org", 10, false, remember);
  }

  function checkSettingDialog(aIsPrivateWindow, aWindow, aCallback) {
    let selectedBrowser = aWindow.gBrowser.selectedBrowser;

    function onLoad() {
      if (aWindow.content.location.href != TEST_URL) {
        selectedBrowser.loadURI(TEST_URL);
        return;
      }
      selectedBrowser.removeEventListener("load", onLoad, true);
      Services.ww.unregisterNotification(observer);

      ok(aIsPrivateWindow,
         "Confirm setting dialog is not displayed for private window");

      executeSoon(aCallback);
    }
    selectedBrowser.addEventListener("load", onLoad, true);

    function observer(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened")
        return;
      selectedBrowser.removeEventListener("load", onLoad, true);
      Services.ww.unregisterNotification(observer);
      ok(!aIsPrivateWindow,
         "Confirm setting dialog is displayed for normal window");
      let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
      executeSoon(function () {
        info("Wait for window close");
        waitForWindowClose(win, aCallback);
      });
    }
    Services.ww.registerNotification(observer);

    selectedBrowser.loadURI(TEST_URL);
  }

  let windowsToClose = [];
  function testOnWindow(aIsPrivate, aCallback) {
    whenNewWindowLoaded({private: aIsPrivate}, function(aWin) {
      windowsToClose.push(aWin);
      aCallback(aWin);
    });
  }

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("network.cookie.lifetimePolicy");
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  // Ask all cookies
  Services.prefs.setIntPref("network.cookie.lifetimePolicy", 1);

  testOnWindow(true, function(aPrivWin) {
    info("Test on private window");
    checkRememberOption(true, aPrivWin, function() {
      checkSettingDialog(true, aPrivWin, function() {
        testOnWindow(false, function(aWin) {
          info("Test on public window");
          checkRememberOption(false, aWin, function() {
            checkSettingDialog(false, aWin, finish);
          });
        });
      });
    });
  });
}

function waitForWindowClose(aWin, aCallback) {
  function observer(aSubject, aTopic, aData) {
    if (aTopic == "domwindowclosed") {
      info("Window closed");
      Services.ww.unregisterNotification(observer);
      executeSoon(aCallback);
    }
  }
  Services.ww.registerNotification(observer);
  aWin.close();
}
