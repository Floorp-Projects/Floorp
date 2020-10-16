/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL =
  "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsPrompt.html";
const notificationID = "indexedDB-permissions-prompt";

async function doTest(win, originAttributes) {
  removePermission(testPageURL, "indexedDB", originAttributes);

  registerPopupEventHandler(
    "popupshowing",
    function() {
      ok(true, "prompt showing");
    },
    win
  );
  registerPopupEventHandler(
    "popupshown",
    function() {
      ok(true, "prompt shown");
      triggerSecondaryCommand(this, win);
    },
    win
  );
  registerPopupEventHandler(
    "popuphidden",
    function() {
      ok(true, "prompt hidden");
    },
    win
  );

  info("creating tab");
  win.gBrowser.selectedTab = BrowserTestUtils.addTab(win.gBrowser);

  info("loading test page: " + testPageURL);
  BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, testPageURL);
  await waitForMessage("InvalidStateError", win.gBrowser);

  is(
    getPermission(testPageURL, "indexedDB", originAttributes),
    Ci.nsIPermissionManager.DENY_ACTION,
    "Correct permission set"
  );
  // unregisterAllPopupEventHandlers(win);
  win.gBrowser.removeCurrentTab();
}

add_task(async function test1() {
  removePermission(testPageURL, "indexedDB");

  await doTest(window, {});
});

add_task(async function test2() {
  info("creating private window");
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  await doTest(win, { privateBrowsingId: 1 });

  await BrowserTestUtils.closeWindow(win);
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
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, testPageURL);
  await waitForMessage("InvalidStateError", gBrowser);

  is(
    getPermission(testPageURL, "indexedDB"),
    Ci.nsIPermissionManager.DENY_ACTION,
    "Correct permission set"
  );
  gBrowser.removeCurrentTab();
  unregisterAllPopupEventHandlers();
  removePermission(testPageURL, "indexedDB");
});
