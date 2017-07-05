/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL = "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsPrompt.html";
const notificationID = "indexedDB-permissions-prompt";

function waitForMessage(aMessage, browser) {
  return new Promise((resolve, reject) => {
    /* eslint-disable no-undef */
    function contentScript() {
      addEventListener("message", function(event) {
        sendAsyncMessage("testLocal:exception",
          {exception: event.data});
      }, {once: true}, true);
    }
    /* eslint-enable no-undef */

    let script = "data:,(" + contentScript.toString() + ")();";

    let mm = browser.selectedBrowser.messageManager;

    mm.addMessageListener("testLocal:exception", function listener(msg) {
      mm.removeMessageListener("testLocal:exception", listener);
      mm.removeDelayedFrameScript(script);
      is(msg.data.exception, aMessage, "received " + aMessage);
      if (msg.data.exception == aMessage) {
        resolve();
      } else {
        reject();
      }
    });

    mm.loadFrameScript(script, true);
  });
}

add_task(async function test1() {
  removePermission(testPageURL, "indexedDB");

  registerPopupEventHandler("popupshowing", function() {
    ok(true, "prompt showing");
  });
  registerPopupEventHandler("popupshown", function() {
    ok(true, "prompt shown");
    triggerSecondaryCommand(this);
  });
  registerPopupEventHandler("popuphidden", function() {
    ok(true, "prompt hidden");
  });

  info("creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("loading test page: " + testPageURL);
  gBrowser.selectedBrowser.loadURI(testPageURL);
  await waitForMessage("InvalidStateError", gBrowser);

  is(getPermission(testPageURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  gBrowser.removeCurrentTab();
});

add_task(async function test2() {
  info("creating private window");
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  registerPopupEventHandler("popupshowing", function() {
    ok(false, "prompt showing");
  }, win);
  registerPopupEventHandler("popupshown", function() {
    ok(false, "prompt shown");
  }, win);
  registerPopupEventHandler("popuphidden", function() {
    ok(false, "prompt hidden");
  }, win);

  info("creating private tab");
  win.gBrowser.selectedTab = win.gBrowser.addTab();

  info("loading test page: " + testPageURL);
  win.gBrowser.selectedBrowser.loadURI(testPageURL);
  await waitForMessage("InvalidStateError", win.gBrowser);

  is(getPermission(testPageURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  unregisterAllPopupEventHandlers();
  win.gBrowser.removeCurrentTab();
  win.close();
});

add_task(async function test3() {
  registerPopupEventHandler("popupshowing", function() {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popupshown", function() {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popuphidden", function() {
    ok(false, "Shouldn't show a popup this time");
  });

  info("creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("loading test page: " + testPageURL);
  gBrowser.selectedBrowser.loadURI(testPageURL);
  await waitForMessage("InvalidStateError", gBrowser);

  is(getPermission(testPageURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  gBrowser.removeCurrentTab();
  unregisterAllPopupEventHandlers();
  removePermission(testPageURL, "indexedDB");
});
