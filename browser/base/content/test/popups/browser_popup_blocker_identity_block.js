"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const baseURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const URL = baseURL + "popup_blocker2.html";
const URI = Services.io.newURI(URL);
const PRINCIPAL = Services.scriptSecurityManager.createContentPrincipal(
  URI,
  {}
);

function openPermissionPopup() {
  let promise = BrowserTestUtils.waitForEvent(
    window,
    "popupshown",
    true,
    event => event.target == gPermissionPanel._permissionPopup
  );
  gPermissionPanel._identityPermissionBox.click();
  return promise;
}

function closePermissionPopup() {
  let promise = BrowserTestUtils.waitForEvent(
    gPermissionPanel._permissionPopup,
    "popuphidden"
  );
  gPermissionPanel._permissionPopup.hidePopup();
  return promise;
}

add_task(async function enable_popup_blocker() {
  // Enable popup blocker.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.disable_open_during_load", true]],
  });
  await SpecialPowers.pushPrefEnv({
    set: [["dom.disable_open_click_delay", 0]],
  });
});

add_task(async function check_blocked_popup_indicator() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  // Blocked popup indicator should not exist in the identity popup when there are no blocked popups.
  await openPermissionPopup();
  Assert.equal(document.getElementById("blocked-popup-indicator-item"), null);
  await closePermissionPopup();

  // Blocked popup notification icon should be hidden in the identity block when no popups are blocked.
  let icon = gPermissionPanel._identityPermissionBox.querySelector(
    ".blocked-permission-icon[data-permission-id='popup']"
  );
  Assert.equal(icon.hasAttribute("showing"), false);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    let open = content.document.getElementById("pop");
    open.click();
  });

  // Wait for popup block.
  await TestUtils.waitForCondition(() =>
    gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked")
  );

  // Check if blocked popup indicator text is visible in the identity popup. It should be visible.
  document.getElementById("identity-permission-box").click();
  await openPermissionPopup();
  await TestUtils.waitForCondition(
    () => document.getElementById("blocked-popup-indicator-item") !== null
  );

  // Check that the default state is correctly set to "Block".
  let menulist = document.getElementById("permission-popup-menulist");
  Assert.equal(menulist.value, "0");
  Assert.equal(menulist.label, "Block");

  await closePermissionPopup();

  // Check if blocked popup icon is visible in the identity block.
  Assert.equal(icon.getAttribute("showing"), "true");

  gBrowser.removeTab(tab);
});

// Check if clicking on "Show blocked popups" shows blocked popups.
add_task(async function check_popup_showing() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    let open = content.document.getElementById("pop");
    open.click();
  });

  // Wait for popup block.
  await TestUtils.waitForCondition(() =>
    gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked")
  );

  // Store the popup that opens in this array.
  let popup;
  function onTabOpen(event) {
    popup = event.target;
  }
  gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen);

  // Open identity popup and click on "Show blocked popups".
  await openPermissionPopup();
  let e = document.getElementById("blocked-popup-indicator-item");
  let text = e.getElementsByTagName("label")[0];
  text.click();

  await BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  await TestUtils.waitForCondition(
    () => popup.linkedBrowser.currentURI.spec != "about:blank"
  );

  gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen);

  ok(
    popup.linkedBrowser.currentURI.spec.endsWith("popup_blocker_a.html"),
    "Popup a"
  );

  gBrowser.removeTab(popup);
  gBrowser.removeTab(tab);
});

// Test if changing menulist values of blocked popup indicator changes permission state and popup behavior.
add_task(async function check_permission_state_change() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  // Initially the permission state is BLOCK for popups (set by the prefs).
  let state = SitePermissions.getForPrincipal(PRINCIPAL, "popup", gBrowser)
    .state;
  Assert.equal(state, SitePermissions.BLOCK);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    let open = content.document.getElementById("pop");
    open.click();
  });

  // Wait for popup block.
  await TestUtils.waitForCondition(() =>
    gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked")
  );

  // Open identity popup and change permission state to allow.
  await openPermissionPopup();
  let menulist = document.getElementById("permission-popup-menulist");
  menulist.menupopup.openPopup(); // Open the allow/block menu
  let menuitem = menulist.getElementsByTagName("menuitem")[0];
  menuitem.click();
  await closePermissionPopup();

  state = SitePermissions.getForPrincipal(PRINCIPAL, "popup", gBrowser).state;
  Assert.equal(state, SitePermissions.ALLOW);

  // Store the popup that opens in this array.
  let popup;
  function onTabOpen(event) {
    popup = event.target;
  }
  gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen);

  // Check if a popup opens.
  await Promise.all([
    SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
      let open = content.document.getElementById("pop");
      open.click();
    }),
    BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen"),
  ]);
  await TestUtils.waitForCondition(
    () => popup.linkedBrowser.currentURI.spec != "about:blank"
  );

  gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen);

  ok(
    popup.linkedBrowser.currentURI.spec.endsWith("popup_blocker_a.html"),
    "Popup a"
  );

  gBrowser.removeTab(popup);

  // Open identity popup and change permission state to block.
  await openPermissionPopup();
  menulist = document.getElementById("permission-popup-menulist");
  menulist.menupopup.openPopup(); // Open the allow/block menu
  menuitem = menulist.getElementsByTagName("menuitem")[1];
  menuitem.click();
  await closePermissionPopup();

  // Clicking on the "Block" menuitem should remove the permission object(same behavior as UNKNOWN state).
  // We have already confirmed that popups are blocked when the permission state is BLOCK.
  state = SitePermissions.getForPrincipal(PRINCIPAL, "popup", gBrowser).state;
  Assert.equal(state, SitePermissions.BLOCK);

  gBrowser.removeTab(tab);
});

// Explicitly set the permission to the otherwise default state and check that
// the label still displays correctly.
add_task(async function check_explicit_default_permission() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  // DENY only works if triggered through Services.perms (it's very edge-casey),
  // since SitePermissions.jsm considers setting default permissions to be removal.
  PermissionTestUtils.add(URI, "popup", Ci.nsIPermissionManager.DENY_ACTION);

  await openPermissionPopup();
  let menulist = document.getElementById("permission-popup-menulist");
  Assert.equal(menulist.value, "0");
  Assert.equal(menulist.label, "Block");
  await closePermissionPopup();

  PermissionTestUtils.add(URI, "popup", Services.perms.ALLOW_ACTION);

  await openPermissionPopup();
  menulist = document.getElementById("permission-popup-menulist");
  Assert.equal(menulist.value, "1");
  Assert.equal(menulist.label, "Allow");
  await closePermissionPopup();

  PermissionTestUtils.remove(URI, "popup");
  gBrowser.removeTab(tab);
});
