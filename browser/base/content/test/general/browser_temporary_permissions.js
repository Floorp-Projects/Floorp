/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource:///modules/SitePermissions.jsm", this);
Cu.import("resource:///modules/E10SUtils.jsm");

const SUBFRAME_PAGE = "https://example.com/browser/browser/base/content/test/general/temporary_permissions_subframe.html";

// Test that setting temp permissions triggers a change in the identity block.
add_task(function* testTempPermissionChangeEvents() {
  let uri = NetUtil.newURI("https://example.com");
  let id = "geo";

  yield BrowserTestUtils.withNewTab(uri.spec, function*(browser) {
    SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    let geoIcon = document.querySelector(".blocked-permission-icon[data-permission-id=geo]");

    Assert.notEqual(geoIcon.boxObject.width, 0, "geo anchor should be visible");

    SitePermissions.remove(uri, id, browser);

    Assert.equal(geoIcon.boxObject.width, 0, "geo anchor should not be visible");
  });
});

// Test that temp permissions are persisted through moving tabs to new windows.
add_task(function* testTempPermissionOnTabMove() {
  let uri = NetUtil.newURI("https://example.com");
  let id = "geo";

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, uri.spec);

  SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, tab.linkedBrowser);

  Assert.deepEqual(SitePermissions.get(uri, id, tab.linkedBrowser), {
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_TEMPORARY,
  });

  let promiseWin = BrowserTestUtils.waitForNewWindow();
  gBrowser.replaceTabWithWindow(tab);
  let win = yield promiseWin;
  tab = win.gBrowser.selectedTab;

  Assert.deepEqual(SitePermissions.get(uri, id, tab.linkedBrowser), {
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_TEMPORARY,
  });

  SitePermissions.remove(uri, id, tab.linkedBrowser);
  yield BrowserTestUtils.closeWindow(win);
});

// Test that temp permissions don't affect other tabs of the same URI.
add_task(function* testTempPermissionMultipleTabs() {
  let uri = NetUtil.newURI("https://example.com");
  let id = "geo";

  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, uri.spec);
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, uri.spec);

  SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, tab2.linkedBrowser);

  Assert.deepEqual(SitePermissions.get(uri, id, tab2.linkedBrowser), {
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_TEMPORARY,
  });

  Assert.deepEqual(SitePermissions.get(uri, id, tab1.linkedBrowser), {
    state: SitePermissions.UNKNOWN,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  let geoIcon = document.querySelector(".blocked-permission-icon[data-permission-id=geo]");

  Assert.notEqual(geoIcon.boxObject.width, 0, "geo anchor should be visible");

  yield BrowserTestUtils.switchTab(gBrowser, tab1);

  Assert.equal(geoIcon.boxObject.width, 0, "geo anchor should not be visible");

  SitePermissions.remove(uri, id, tab2.linkedBrowser);
  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
});

// Test that temp blocked permissions requested by subframes (with a different URI) affect the whole page.
add_task(function* testTempPermissionSubframes() {
  let uri = NetUtil.newURI("https://example.com");
  let id = "geo";

  yield BrowserTestUtils.withNewTab(SUBFRAME_PAGE, function*(browser) {
    let popupshown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");

    // Request a permission;
    yield ContentTask.spawn(browser, uri.host, function(host) {
      E10SUtils.wrapHandlingUserInput(content, true, function() {
        let frame = content.document.getElementById("frame");
        let frameDoc = frame.contentWindow.document;

        // Make sure that the origin of our test page is different.
        Assert.notEqual(frameDoc.location.host, host);

        frameDoc.getElementById("geo").click();
      });
    });

    yield popupshown;

    let popuphidden = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");

    let notification = PopupNotifications.panel.firstChild;
    EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});

    yield popuphidden;

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });
  });
});
