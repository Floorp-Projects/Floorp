/**
 * This test is used to ensure the value of temporary 'autoplay-media' permission
 * is sync correctly between JS side (TemporaryPermissions) and C++ side
 * (nsPIDOMWindowOuter). When the value of temporary permission changes in JS
 * side, the C++ side would be notified and cache the result of allowing autoplay.
 */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

ChromeUtils.import("resource://gre/modules/E10SUtils.jsm");

const ORIGIN = "https://example.com";
const URI = Services.io.newURI(ORIGIN);
const PERMISSIONS_PAGE = getRootDirectory(gTestPath).replace("chrome://mochitests/content", ORIGIN) + "permissions.html";
const SUBFRAME_PAGE = getRootDirectory(gTestPath).replace("chrome://mochitests/content", ORIGIN) + "temporary_permissions_subframe.html";
const ID = "autoplay-media";

async function checkTemporaryPermission(browser, state, scope) {
  Assert.deepEqual(SitePermissions.get(URI, ID, browser), {state, scope});
  let isAllowed = state == SitePermissions.ALLOW;
  await ContentTask.spawn(browser, isAllowed,
    isTemporaryAllowed => {
      is(content.windowUtils.isAutoplayTemporarilyAllowed(), isTemporaryAllowed,
         "temporary autoplay permission is as same as what we cached in window.");
    });
}

add_task(async function setTestingPreferences() {
  return SpecialPowers.pushPrefEnv({"set": [
    ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.PROMPT],
    ["media.autoplay.enabled.user-gestures-needed", true],
    ["media.autoplay.ask-permission", true],
  ]});
});

// Test that temp blocked permissions requested by subframes (with a different URI) affect the whole page.
add_task(async function testTempPermissionSubframes() {
  await BrowserTestUtils.withNewTab(SUBFRAME_PAGE, async function(browser) {
    let popupshown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");

    info(`- request permission from subframe -`);
    await ContentTask.spawn(browser, URI.host, function(host) {
      E10SUtils.wrapHandlingUserInput(content, true, function() {
        let frame = content.document.getElementById("frame");
        let frameDoc = frame.contentWindow.document;

        // Make sure that the origin of our test page is different.
        Assert.notEqual(frameDoc.location.host, host);
        frameDoc.getElementById("autoplay").click();
      });
    });
    await popupshown;
    let popuphidden = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");

    info(`- click 'allow' button and uncheck the checkbox-`);
    let notification = PopupNotifications.panel.firstChild;
    notification.checkbox.checked = false;
    EventUtils.synthesizeMouseAtCenter(notification.button, {});
    await popuphidden;

    info(`- should get the temporary permission -`);
    checkTemporaryPermission(browser, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY);
  });
});

// Test that temporary permissions are removed on user initiated reload only.
add_task(async function testTempPermissionOnReload() {
  await BrowserTestUtils.withNewTab(URI.spec, async function(browser) {
    info(`- set temporary permission -`);
    SitePermissions.set(URI, ID, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY, browser);
    checkTemporaryPermission(browser, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY);

    info(`- reload through the page -`);
    // Reload through the page (should not remove the temp permission).
    await ContentTask.spawn(browser, {}, () => content.document.location.reload());
    let reloadButton = document.getElementById("reload-button");
    let reloaded = BrowserTestUtils.browserLoaded(browser, false, URI.spec);
    await reloaded;
    await BrowserTestUtils.waitForCondition(() => {
      return !reloadButton.disabled;
    });

    info(`- the temporary permission should not be removed -`);
    checkTemporaryPermission(browser, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY);

    info(`- reload as a user -`);
    reloaded = BrowserTestUtils.browserLoaded(browser, false, URI.spec);
    // Reload as a user (should remove the temp permission).
    EventUtils.synthesizeMouseAtCenter(reloadButton, {});
    await reloaded;

    info(`- the temporary permission should be removed -`);
    checkTemporaryPermission(browser, SitePermissions.UNKNOWN, SitePermissions.SCOPE_PERSISTENT);

    info(`- set temporary permission again -`);
    SitePermissions.set(URI, ID, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

    info(`- open the tab context menu -`);
    openTabContextMenu();

    info(`- reload as a user through the context menu -`);
    let reloadMenuItem = document.getElementById("context_reloadTab");
    reloaded = BrowserTestUtils.browserLoaded(browser, false, URI.spec);
    // Reload as a user through the context menu (should remove the temp permission).
    EventUtils.synthesizeMouseAtCenter(reloadMenuItem, {});
    await reloaded;

    info(`- the temporary permission should be removed -`);
    checkTemporaryPermission(browser, SitePermissions.UNKNOWN, SitePermissions.SCOPE_PERSISTENT);

    SitePermissions.remove(URI, ID, browser);
  });
});

// Test that temporary permissions are not removed when reloading all tabs.
add_task(async function testTempPermissionOnReloadAllTabs() {
  await BrowserTestUtils.withNewTab(URI.spec, async function(browser) {
    info(`- set temporary permission -`);
    SitePermissions.set(URI, ID, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY, browser);
    // Select all tabs before opening the context menu.
    gBrowser.selectAllTabs();

    info(`- open the tab context menu -`);
    openTabContextMenu();

    info(`- reload all tabs -`);
    let reloadMenuItem = document.getElementById("context_reloadSelectedTabs");
    let reloaded = Promise.all(gBrowser.visibleTabs.map(
      tab => BrowserTestUtils.browserLoaded(gBrowser.getBrowserForTab(tab))));
    EventUtils.synthesizeMouseAtCenter(reloadMenuItem, {});
    await reloaded;

    info(`- the temporary permissions for the current URI should not be removed -`);
    checkTemporaryPermission(browser, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY);

    SitePermissions.remove(URI, ID, browser);
  });
});

// Test that temporary permissions are persisted through navigation in a tab.
add_task(async function testTempPermissionOnNavigation() {
  await BrowserTestUtils.withNewTab(URI.spec, async function(browser) {
    info(`- set temporary permission -`);
    SitePermissions.set(URI, ID, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY, browser);

    info(`- check temporary permission -`);
    checkTemporaryPermission(browser, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY);

    info(`- navigate to another domain -`);
    let loaded = BrowserTestUtils.browserLoaded(browser, false, "https://example.org/");
    await ContentTask.spawn(browser, {}, () => content.document.location = "https://example.org/");
    await loaded;

    info(`- the temporary permissions for the current URI should be reset -`);
    checkTemporaryPermission(browser, SitePermissions.UNKNOWN, SitePermissions.SCOPE_PERSISTENT);

    info(`- navigate to the original domain -`);
    loaded = BrowserTestUtils.browserLoaded(browser, false, URI.spec);
    await ContentTask.spawn(browser, {}, () => content.document.location = "https://example.com/");
    await loaded;

    info(`- the temporary permissions for the original URI should still exist -`);
    checkTemporaryPermission(browser, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY);

    SitePermissions.remove(URI, ID, browser);
  });
});

// Test that temporary permissions can be re-requested after they expired
// and that the identity block is updated accordingly.
add_task(async function testTempPermissionRequestAfterExpiry() {
  const TIMEOUT_MS = 100;
  await SpecialPowers.pushPrefEnv({set: [
    ["privacy.temporary_permission_expire_time_ms", TIMEOUT_MS],
  ]});

  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(browser) {
    info(`- set temporary permission -`);
    SitePermissions.set(URI, ID, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY, browser);

    info(`- check temporary permission -`);
    checkTemporaryPermission(browser, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY);

    info(`- wait for the permission expiration -`);
    await new Promise((c) => setTimeout(c, TIMEOUT_MS));

    info(`- the temporary permission should be removed -`);
    checkTemporaryPermission(browser, SitePermissions.UNKNOWN, SitePermissions.SCOPE_PERSISTENT);

    SitePermissions.remove(URI, ID, browser);
  });
});
