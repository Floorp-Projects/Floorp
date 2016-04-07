/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL = "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsPrompt.html";
const notificationID = "indexedDB-permissions-prompt";

function setUsePrivateBrowsing(browser, val) {
  if (!browser.isRemoteBrowser) {
    browser.docShell.QueryInterface(Ci.nsILoadContext).usePrivateBrowsing = val;
    return;
  }

  return ContentTask.spawn(browser, val, function* (val) {
    docShell.QueryInterface(Ci.nsILoadContext).usePrivateBrowsing = val;
  });
};


function promiseMessage(aMessage) {
  return ContentTask.spawn(gBrowser.selectedBrowser, aMessage, function* (aMessage) {
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
    triggerSecondaryCommand(this, 0);
  });
  registerPopupEventHandler("popuphidden", function () {
    ok(true, "prompt hidden");
  });

  yield promiseMessage("InvalidStateError");

  is(getPermission(testPageURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  gBrowser.removeCurrentTab();
});

add_task(function test2() {
  info("creating private tab");
  gBrowser.selectedTab = gBrowser.addTab();
  yield setUsePrivateBrowsing(gBrowser.selectedBrowser, true);

  info("loading test page: " + testPageURL);
  gBrowser.selectedBrowser.loadURI(testPageURL);
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerPopupEventHandler("popupshowing", function () {
    ok(false, "prompt showing");
  });
  registerPopupEventHandler("popupshown", function () {
    ok(false, "prompt shown");
  });
  registerPopupEventHandler("popuphidden", function () {
    ok(false, "prompt hidden");
  });

  yield promiseMessage("InvalidStateError");

  is(getPermission(testPageURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  unregisterAllPopupEventHandlers();
  yield setUsePrivateBrowsing(gBrowser.selectedBrowser, false);
  gBrowser.removeCurrentTab();
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

  yield promiseMessage("InvalidStateError");

  is(getPermission(testPageURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  gBrowser.removeCurrentTab();
  unregisterAllPopupEventHandlers();
  removePermission(testPageURL, "indexedDB");
});
