/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL = "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsPrompt.html";
const notificationID = "indexedDB-permissions-prompt";

function promiseMessage(aMessage, browser) {
  return ContentTask.spawn(browser.selectedBrowser, aMessage, function* (aMessage) {
    yield new Promise((resolve, reject) => {
      content.addEventListener("message", function messageListener(event) {
        content.removeEventListener("message", messageListener);
        is(event.data, aMessage, "received " + aMessage);
        if (event.data == aMessage)
          resolve();
        else
          reject();
      });
    });
  });
}

add_task(function test1() {
  removePermission(testPageURL, "indexedDB");

  info("creating tab");
  gBrowser.selectedTab = gBrowser.addTab();

  info("loading test page: " + testPageURL);
  gBrowser.selectedBrowser.loadURI(testPageURL);
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerPopupEventHandler("popupshowing", function () {
    ok(true, "prompt showing");
  });
  registerPopupEventHandler("popupshown", function () {
    ok(true, "prompt shown");
    triggerSecondaryCommand(this);
  });
  registerPopupEventHandler("popuphidden", function () {
    ok(true, "prompt hidden");
  });

  yield promiseMessage("InvalidStateError", gBrowser);

  is(getPermission(testPageURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  gBrowser.removeCurrentTab();
});

add_task(function test2() {
  info("creating private window");
  let win = yield BrowserTestUtils.openNewBrowserWindow({ private : true });
  
  info("creating private tab");
  win.gBrowser.selectedTab = win.gBrowser.addTab();

  info("loading test page: " + testPageURL);
  win.gBrowser.selectedBrowser.loadURI(testPageURL);
  yield BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  
  registerPopupEventHandler("popupshowing", function () {
    ok(false, "prompt showing");
  });
  registerPopupEventHandler("popupshown", function () {
    ok(false, "prompt shown");
  });
  registerPopupEventHandler("popuphidden", function () {
    ok(false, "prompt hidden");
  });
  yield promiseMessage("InvalidStateError", win.gBrowser);

  is(getPermission(testPageURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  unregisterAllPopupEventHandlers();
  win.gBrowser.removeCurrentTab();
  win.close();
});

add_task(function test3() {
  info("creating tab");
  gBrowser.selectedTab = gBrowser.addTab();

  info("loading test page: " + testPageURL);
  gBrowser.selectedBrowser.loadURI(testPageURL);
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerPopupEventHandler("popupshowing", function () {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popupshown", function () {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popuphidden", function () {
    ok(false, "Shouldn't show a popup this time");
  });

  yield promiseMessage("InvalidStateError", gBrowser);

  is(getPermission(testPageURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  gBrowser.removeCurrentTab();
  unregisterAllPopupEventHandlers();
  removePermission(testPageURL, "indexedDB");
});
