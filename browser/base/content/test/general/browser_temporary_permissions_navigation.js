/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource:///modules/SitePermissions.jsm", this);

// Test that temporary permissions are removed on user initiated reload only.
add_task(function* testTempPermissionOnReload() {
  let uri = NetUtil.newURI("https://example.com");
  let id = "geo";

  yield BrowserTestUtils.withNewTab(uri.spec, function*(browser) {
    let reloadButton = document.getElementById("urlbar-reload-button");

    SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

    let reloaded = BrowserTestUtils.browserLoaded(browser, false, uri.spec);

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    // Reload through the page (should not remove the temp permission).
    yield ContentTask.spawn(browser, {}, () => content.document.location.reload());

    yield reloaded;
    yield BrowserTestUtils.waitForCondition(() => {
      return reloadButton.disabled == false;
    });

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    // Reload as a user (should remove the temp permission).
    EventUtils.synthesizeMouseAtCenter(reloadButton, {});

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    });

    SitePermissions.remove(uri, id, browser);
  });
});

// Test that temporary permissions are persisted through navigation in a tab.
add_task(function* testTempPermissionOnNavigation() {
  let uri = NetUtil.newURI("https://example.com/");
  let id = "geo";

  yield BrowserTestUtils.withNewTab(uri.spec, function*(browser) {
    SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    let loaded = BrowserTestUtils.browserLoaded(browser, false, "https://example.org/");

    // Navigate to another domain.
    yield ContentTask.spawn(browser, {}, () => content.document.location = "https://example.org/");

    yield loaded;

    // The temporary permissions for the current URI should be reset.
    Assert.deepEqual(SitePermissions.get(browser.currentURI, id, browser), {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    });

    loaded = BrowserTestUtils.browserLoaded(browser, false, uri.spec);

    // Navigate to the original domain.
    yield ContentTask.spawn(browser, {}, () => content.document.location = "https://example.com/");

    yield loaded;

    // The temporary permissions for the original URI should still exist.
    Assert.deepEqual(SitePermissions.get(browser.currentURI, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    SitePermissions.remove(uri, id, browser);
  });
});

