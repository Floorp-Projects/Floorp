/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that temporary permissions are removed on user initiated reload only.
add_task(async function testTempPermissionOnReload() {
  let uri = NetUtil.newURI("https://example.com");
  let id = "geo";

  await BrowserTestUtils.withNewTab(uri.spec, async function(browser) {
    let reloadButton = document.getElementById("reload-button");

    SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

    let reloaded = BrowserTestUtils.browserLoaded(browser, false, uri.spec);

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    // Reload through the page (should not remove the temp permission).
    await ContentTask.spawn(browser, {}, () => content.document.location.reload());

    await reloaded;
    await BrowserTestUtils.waitForCondition(() =>
      !reloadButton.disabled && !reloadButton.hasAttribute("temporarily-disabled"));

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    reloaded = BrowserTestUtils.browserLoaded(browser, false, uri.spec);

    // Reload as a user (should remove the temp permission).
    EventUtils.synthesizeMouseAtCenter(reloadButton, {});

    await reloaded;

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    });

    // Set the permission again.
    SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

    // Open the tab context menu.
    let contextMenu = document.getElementById("tabContextMenu");
    let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(gBrowser.selectedTab, {type: "contextmenu", button: 2});
    await popupShownPromise;

    let reloadMenuItem = document.getElementById("context_reloadTab");

    reloaded = BrowserTestUtils.browserLoaded(browser, false, uri.spec);

    // Reload as a user through the context menu (should remove the temp permission).
    EventUtils.synthesizeMouseAtCenter(reloadMenuItem, {});

    await reloaded;

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    });

    SitePermissions.remove(uri, id, browser);
  });
});

// Test that temporary permissions are not removed when reloading all tabs.
add_task(async function testTempPermissionOnReloadAllTabs() {
  let uri = NetUtil.newURI("https://example.com");
  let id = "geo";

  await BrowserTestUtils.withNewTab(uri.spec, async function(browser) {
    SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

    // Open the tab context menu.
    let contextMenu = document.getElementById("tabContextMenu");
    let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(gBrowser.selectedTab, {type: "contextmenu", button: 2});
    await popupShownPromise;

    let reloadMenuItem = document.getElementById("context_reloadAllTabs");

    let reloaded = Promise.all(gBrowser.visibleTabs.map(
      tab => BrowserTestUtils.browserLoaded(gBrowser.getBrowserForTab(tab))));
    EventUtils.synthesizeMouseAtCenter(reloadMenuItem, {});
    await reloaded;

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    SitePermissions.remove(uri, id, browser);
  });
});

// Test that temporary permissions are persisted through navigation in a tab.
add_task(async function testTempPermissionOnNavigation() {
  let uri = NetUtil.newURI("https://example.com/");
  let id = "geo";

  await BrowserTestUtils.withNewTab(uri.spec, async function(browser) {
    SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    let loaded = BrowserTestUtils.browserLoaded(browser, false, "https://example.org/");

    // Navigate to another domain.
    await ContentTask.spawn(browser, {}, () => content.document.location = "https://example.org/");

    await loaded;

    // The temporary permissions for the current URI should be reset.
    Assert.deepEqual(SitePermissions.get(browser.currentURI, id, browser), {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    });

    loaded = BrowserTestUtils.browserLoaded(browser, false, uri.spec);

    // Navigate to the original domain.
    await ContentTask.spawn(browser, {}, () => content.document.location = "https://example.com/");

    await loaded;

    // The temporary permissions for the original URI should still exist.
    Assert.deepEqual(SitePermissions.get(browser.currentURI, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    SitePermissions.remove(uri, id, browser);
  });
});

