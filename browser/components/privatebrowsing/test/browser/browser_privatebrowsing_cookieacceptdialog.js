/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that private browsing mode disables the "remember"
// option in the cookie accept dialog.

add_task(function* test() {
  // initialization
  const TEST_URL = "http://mochi.test:8888/browser/browser/components/" +
                   "privatebrowsing/test/browser/" +
                   "browser_privatebrowsing_cookieacceptdialog.html";
  const BLANK_URL = "http://mochi.test:8888/";
  let cp = Cc["@mozilla.org/embedcomp/cookieprompt-service;1"].
           getService(Ci.nsICookiePromptService);


  function openCookieDialog(aWindow) {
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

    executeSoon(function () {
      cp.cookieDialog(aWindow, cookie, "mozilla.org", 10, false, remember);
    });
    return BrowserTestUtils.domWindowOpened();
  };


  function checkRememberOption(expectedDisabled, aWindow) {
    return Task.spawn(function* () {
      let dialogWin = yield openCookieDialog(aWindow);

      yield new Promise(resolve => {
        dialogWin.addEventListener("load", function onLoad(event) {
          dialogWin.removeEventListener("load", onLoad, false);
          resolve();
        }, false);
      });

      let doc = dialogWin.document;
      let remember = doc.getElementById("persistDomainAcceptance");
      ok(remember, "The remember checkbox should exist");

      if (expectedDisabled)
        is(remember.getAttribute("disabled"), "true",
           "The checkbox should be disabled");
      else
        ok(!remember.hasAttribute("disabled"),
           "The checkbox should not be disabled");

      yield BrowserTestUtils.closeWindow(dialogWin);
    });
  };

  function checkSettingDialog(aIsPrivateWindow, aWindow) {
    return Task.spawn(function* () {
      let dialogOpened = false;
      let promiseDialogClosed = null;

      function observer(subject, topic, data) {
        if (topic != "domwindowopened") { return; }
        Services.ww.unregisterNotification(observer);
        dialogOpened = true;

        promiseDialogClosed = BrowserTestUtils.closeWindow(
          subject.QueryInterface(Ci.nsIDOMWindow));
      }
      Services.ww.registerNotification(observer);

      let selectedBrowser = aWindow.gBrowser.selectedBrowser;
      selectedBrowser.loadURI(TEST_URL);
      yield BrowserTestUtils.browserLoaded(selectedBrowser);;

      if (dialogOpened) {
        ok(!aIsPrivateWindow,
           "Setting dialog shown, confirm normal window");
      } else {
        Services.ww.unregisterNotification(observer);
        ok(aIsPrivateWindow,
           "Confirm setting dialog is not displayed for private window");
      }

      yield promiseDialogClosed;
    });
  };

  // Ask all cookies
  Services.prefs.setIntPref("network.cookie.lifetimePolicy", 1);

  let win = yield BrowserTestUtils.openNewBrowserWindow();
  info("Test on public window");

  yield checkRememberOption(false, win);
  yield checkSettingDialog(false, win);

  let privateWin = yield BrowserTestUtils.openNewBrowserWindow({private: true});
  info("Test on private window");

  yield checkRememberOption(true, privateWin);
  yield checkSettingDialog(true, privateWin);


  // Cleanup
  Services.prefs.clearUserPref("network.cookie.lifetimePolicy");
  yield BrowserTestUtils.closeWindow(win);
  yield BrowserTestUtils.closeWindow(privateWin);
});
