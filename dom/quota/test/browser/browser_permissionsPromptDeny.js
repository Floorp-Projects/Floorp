/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL =
  "https://example.com/browser/dom/quota/test/browser/permissionsPrompt.html";

addTest(async function testPermissionTemporaryDenied() {
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

  info("Creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Loading test page: " + testPageURL);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, testPageURL);
  await waitForMessage(false, gBrowser);

  is(
    getPermission(testPageURL, "persistent-storage"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    "Correct permission set"
  );

  let tempBlock = SitePermissions.getAllForBrowser(
    gBrowser.selectedBrowser
  ).find(
    p =>
      p.id == "persistent-storage" &&
      p.state == SitePermissions.BLOCK &&
      p.scope == SitePermissions.SCOPE_TEMPORARY
  );
  ok(tempBlock, "Should have a temporary block permission on active browser");

  unregisterAllPopupEventHandlers();
  gBrowser.removeCurrentTab();
  removePermission(testPageURL, "persistent-storage");
});

addTest(async function testPermissionDenied() {
  removePermission(testPageURL, "persistent-storage");

  registerPopupEventHandler("popupshowing", function() {
    ok(true, "prompt showing");
  });
  registerPopupEventHandler("popupshown", function() {
    ok(true, "prompt shown");
    triggerSecondaryCommand(this, 1);
  });
  registerPopupEventHandler("popuphidden", function() {
    ok(true, "prompt hidden");
  });

  info("Creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Loading test page: " + testPageURL);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, testPageURL);
  await waitForMessage(false, gBrowser);

  is(
    getPermission(testPageURL, "persistent-storage"),
    Ci.nsIPermissionManager.DENY_ACTION,
    "Correct permission set"
  );
  unregisterAllPopupEventHandlers();
  gBrowser.removeCurrentTab();
  // Keep persistent-storage permission for the next test.
});

addTest(async function testNoPermissionPrompt() {
  registerPopupEventHandler("popupshowing", function() {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popupshown", function() {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popuphidden", function() {
    ok(false, "Shouldn't show a popup this time");
  });

  info("Creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Loading test page: " + testPageURL);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, testPageURL);
  await waitForMessage(false, gBrowser);

  is(
    getPermission(testPageURL, "persistent-storage"),
    Ci.nsIPermissionManager.DENY_ACTION,
    "Correct permission set"
  );
  unregisterAllPopupEventHandlers();
  gBrowser.removeCurrentTab();
  removePermission(testPageURL, "persistent-storage");
});

addTest(async function testPermissionDeniedDismiss() {
  registerPopupEventHandler("popupshowing", function() {
    ok(true, "prompt showing");
  });
  registerPopupEventHandler("popupshown", function() {
    ok(true, "prompt shown");
    // Dismiss permission prompt.
    dismissNotification(this);
  });
  registerPopupEventHandler("popuphidden", function() {
    ok(true, "prompt hidden");
  });

  info("Creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Loading test page: " + testPageURL);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, testPageURL);
  await waitForMessage(false, gBrowser);

  // Pressing ESC results in a temporary block permission on the browser object.
  // So the global permission for the URL should still be unknown, but the browser
  // should have a block permission with a temporary scope.
  is(
    getPermission(testPageURL, "persistent-storage"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    "Correct permission set"
  );

  let tempBlock = SitePermissions.getAllForBrowser(
    gBrowser.selectedBrowser
  ).find(
    p =>
      p.id == "persistent-storage" &&
      p.state == SitePermissions.BLOCK &&
      p.scope == SitePermissions.SCOPE_TEMPORARY
  );
  ok(tempBlock, "Should have a temporary block permission on active browser");

  unregisterAllPopupEventHandlers();
  gBrowser.removeCurrentTab();
  removePermission(testPageURL, "persistent-storage");
});
