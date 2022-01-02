/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that temp permissions are persisted through moving tabs to new windows.
add_task(async function testTempPermissionOnTabMove() {
  let origin = "https://example.com/";
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    origin
  );
  let id = "geo";

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, origin);

  SitePermissions.setForPrincipal(
    principal,
    id,
    SitePermissions.BLOCK,
    SitePermissions.SCOPE_TEMPORARY,
    tab.linkedBrowser
  );

  Assert.deepEqual(
    SitePermissions.getForPrincipal(principal, id, tab.linkedBrowser),
    {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    }
  );

  let promiseWin = BrowserTestUtils.waitForNewWindow();
  gBrowser.replaceTabWithWindow(tab);
  let win = await promiseWin;
  tab = win.gBrowser.selectedTab;

  Assert.deepEqual(
    SitePermissions.getForPrincipal(principal, id, tab.linkedBrowser),
    {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    }
  );

  SitePermissions.removeFromPrincipal(principal, id, tab.linkedBrowser);
  await BrowserTestUtils.closeWindow(win);
});

// Test that temp permissions don't affect other tabs of the same URI.
add_task(async function testTempPermissionMultipleTabs() {
  let origin = "https://example.com/";
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    origin
  );
  let id = "geo";

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, origin);
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, origin);

  SitePermissions.setForPrincipal(
    principal,
    id,
    SitePermissions.BLOCK,
    SitePermissions.SCOPE_TEMPORARY,
    tab2.linkedBrowser
  );

  Assert.deepEqual(
    SitePermissions.getForPrincipal(principal, id, tab2.linkedBrowser),
    {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    }
  );

  Assert.deepEqual(
    SitePermissions.getForPrincipal(principal, id, tab1.linkedBrowser),
    {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    }
  );

  let geoIcon = document.querySelector(
    ".blocked-permission-icon[data-permission-id=geo]"
  );

  Assert.notEqual(
    geoIcon.getBoundingClientRect().width,
    0,
    "geo anchor should be visible"
  );

  await BrowserTestUtils.switchTab(gBrowser, tab1);

  Assert.equal(
    geoIcon.getBoundingClientRect().width,
    0,
    "geo anchor should not be visible"
  );

  SitePermissions.removeFromPrincipal(principal, id, tab2.linkedBrowser);
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// Test that temp permissions are cleared when closing tabs.
add_task(async function testTempPermissionOnTabClose() {
  let origin = "https://example.com/";
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    origin
  );
  let id = "geo";

  ok(
    !SitePermissions._temporaryPermissions._stateByBrowser.size,
    "Temporary permission map should be empty initially."
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, origin);

  SitePermissions.setForPrincipal(
    principal,
    id,
    SitePermissions.BLOCK,
    SitePermissions.SCOPE_TEMPORARY,
    tab.linkedBrowser
  );

  Assert.deepEqual(
    SitePermissions.getForPrincipal(principal, id, tab.linkedBrowser),
    {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    }
  );

  ok(
    SitePermissions._temporaryPermissions._stateByBrowser.has(
      tab.linkedBrowser
    ),
    "Temporary permission map should have an entry for the browser."
  );

  BrowserTestUtils.removeTab(tab);

  ok(
    !SitePermissions._temporaryPermissions._stateByBrowser.size,
    "Temporary permission map should be empty after closing the tab."
  );
});
