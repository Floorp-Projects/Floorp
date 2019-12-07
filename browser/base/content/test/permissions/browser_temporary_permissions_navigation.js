/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that temporary permissions are removed on user initiated reload only.
add_task(async function testTempPermissionOnReload() {
  let origin = "https://example.com/";
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    origin
  );
  let id = "geo";

  await BrowserTestUtils.withNewTab(origin, async function(browser) {
    let reloadButton = document.getElementById("reload-button");

    SitePermissions.setForPrincipal(
      principal,
      id,
      SitePermissions.BLOCK,
      SitePermissions.SCOPE_TEMPORARY,
      browser
    );

    let reloaded = BrowserTestUtils.browserLoaded(browser, false, origin);

    Assert.deepEqual(SitePermissions.getForPrincipal(principal, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    // Reload through the page (should not remove the temp permission).
    await ContentTask.spawn(browser, {}, () =>
      content.document.location.reload()
    );

    await reloaded;
    await BrowserTestUtils.waitForCondition(() => {
      return !reloadButton.disabled;
    });

    Assert.deepEqual(SitePermissions.getForPrincipal(principal, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    reloaded = BrowserTestUtils.browserLoaded(browser, false, origin);

    // Reload as a user (should remove the temp permission).
    EventUtils.synthesizeMouseAtCenter(reloadButton, {});

    await reloaded;

    Assert.deepEqual(SitePermissions.getForPrincipal(principal, id, browser), {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    });

    // Set the permission again.
    SitePermissions.setForPrincipal(
      principal,
      id,
      SitePermissions.BLOCK,
      SitePermissions.SCOPE_TEMPORARY,
      browser
    );

    // Open the tab context menu.
    let contextMenu = document.getElementById("tabContextMenu");
    // The TabContextMenu initializes its strings only on a focus or mouseover event.
    // Calls focus event on the TabContextMenu early in the test.
    gBrowser.selectedTab.focus();
    let popupShownPromise = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popupshown"
    );
    EventUtils.synthesizeMouseAtCenter(gBrowser.selectedTab, {
      type: "contextmenu",
      button: 2,
    });
    await popupShownPromise;

    let reloadMenuItem = document.getElementById("context_reloadTab");

    reloaded = BrowserTestUtils.browserLoaded(browser, false, origin);

    // Reload as a user through the context menu (should remove the temp permission).
    EventUtils.synthesizeMouseAtCenter(reloadMenuItem, {});

    await reloaded;

    Assert.deepEqual(SitePermissions.getForPrincipal(principal, id, browser), {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    });

    SitePermissions.removeFromPrincipal(principal, id, browser);
  });
});

// Test that temporary permissions are not removed when reloading all tabs.
add_task(async function testTempPermissionOnReloadAllTabs() {
  let origin = "https://example.com/";
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    origin
  );
  let id = "geo";

  await BrowserTestUtils.withNewTab(origin, async function(browser) {
    SitePermissions.setForPrincipal(
      principal,
      id,
      SitePermissions.BLOCK,
      SitePermissions.SCOPE_TEMPORARY,
      browser
    );

    // Select all tabs before opening the context menu.
    gBrowser.selectAllTabs();

    // Open the tab context menu.
    let contextMenu = document.getElementById("tabContextMenu");
    // The TabContextMenu initializes its strings only on a focus or mouseover event.
    // Calls focus event on the TabContextMenu early in the test.
    gBrowser.selectedTab.focus();
    let popupShownPromise = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popupshown"
    );
    EventUtils.synthesizeMouseAtCenter(gBrowser.selectedTab, {
      type: "contextmenu",
      button: 2,
    });
    await popupShownPromise;

    let reloadMenuItem = document.getElementById("context_reloadSelectedTabs");

    let reloaded = Promise.all(
      gBrowser.visibleTabs.map(tab =>
        BrowserTestUtils.browserLoaded(gBrowser.getBrowserForTab(tab))
      )
    );
    EventUtils.synthesizeMouseAtCenter(reloadMenuItem, {});
    await reloaded;

    Assert.deepEqual(SitePermissions.getForPrincipal(principal, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    SitePermissions.removeFromPrincipal(principal, id, browser);
  });
});

// Test that temporary permissions are persisted through navigation in a tab.
add_task(async function testTempPermissionOnNavigation() {
  let origin = "https://example.com/";
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    origin
  );
  let id = "geo";

  await BrowserTestUtils.withNewTab(origin, async function(browser) {
    SitePermissions.setForPrincipal(
      principal,
      id,
      SitePermissions.BLOCK,
      SitePermissions.SCOPE_TEMPORARY,
      browser
    );

    Assert.deepEqual(SitePermissions.getForPrincipal(principal, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    let loaded = BrowserTestUtils.browserLoaded(
      browser,
      false,
      "https://example.org/"
    );

    // Navigate to another domain.
    await ContentTask.spawn(
      browser,
      {},
      () => (content.document.location = "https://example.org/")
    );

    await loaded;

    // The temporary permissions for the current URI should be reset.
    Assert.deepEqual(
      SitePermissions.getForPrincipal(browser.contentPrincipal, id, browser),
      {
        state: SitePermissions.UNKNOWN,
        scope: SitePermissions.SCOPE_PERSISTENT,
      }
    );

    loaded = BrowserTestUtils.browserLoaded(browser, false, origin);

    // Navigate to the original domain.
    await ContentTask.spawn(
      browser,
      {},
      () => (content.document.location = "https://example.com/")
    );

    await loaded;

    // The temporary permissions for the original URI should still exist.
    Assert.deepEqual(
      SitePermissions.getForPrincipal(browser.contentPrincipal, id, browser),
      {
        state: SitePermissions.BLOCK,
        scope: SitePermissions.SCOPE_TEMPORARY,
      }
    );

    SitePermissions.removeFromPrincipal(browser.contentPrincipal, id, browser);
  });
});
