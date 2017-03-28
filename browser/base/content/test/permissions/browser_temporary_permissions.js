/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource:///modules/E10SUtils.jsm");

const ORIGIN = "https://example.com";
const PERMISSIONS_PAGE = getRootDirectory(gTestPath).replace("chrome://mochitests/content", ORIGIN) + "permissions.html";
const SUBFRAME_PAGE = getRootDirectory(gTestPath).replace("chrome://mochitests/content", ORIGIN) + "temporary_permissions_subframe.html";

const EXPIRE_TIME_MS = 100;
const TIMEOUT_MS = 500;

// Test that setting temp permissions triggers a change in the identity block.
add_task(function* testTempPermissionChangeEvents() {
  let uri = NetUtil.newURI(ORIGIN);
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

// Test that temporary permissions can be re-requested after they expired
// and that the identity block is updated accordingly.
// TODO (bug 1349144): Write a check for webrtc permissions,
// because they use a different code path.
add_task(function* testTempPermissionRequestAfterExpiry() {
  yield SpecialPowers.pushPrefEnv({set: [
        ["privacy.temporary_permission_expire_time_ms", EXPIRE_TIME_MS],
  ]});

  let uri = NetUtil.newURI(ORIGIN);
  let id = "geo";

  yield BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, function*(browser) {
    let blockedIcon = gIdentityHandler._identityBox
      .querySelector(`.blocked-permission-icon[data-permission-id='${id}']`);

    SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    ok(blockedIcon.hasAttribute("showing"), "blocked permission icon is shown");

    yield new Promise((c) => setTimeout(c, TIMEOUT_MS));

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    });

    let popupshown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");

    // Request a permission;
    yield BrowserTestUtils.synthesizeMouseAtCenter(`#${id}`, {}, browser);

    yield popupshown;

    ok(!blockedIcon.hasAttribute("showing"), "blocked permission icon is not shown");

    let popuphidden = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");

    let notification = PopupNotifications.panel.firstChild;
    EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});

    yield popuphidden;

    SitePermissions.remove(uri, id, browser);
  });
});

// Test that temp blocked permissions requested by subframes (with a different URI) affect the whole page.
add_task(function* testTempPermissionSubframes() {
  let uri = NetUtil.newURI(ORIGIN);
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
