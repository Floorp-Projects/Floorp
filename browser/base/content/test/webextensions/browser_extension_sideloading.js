const {AddonManagerPrivate} = Cu.import("resource://gre/modules/AddonManager.jsm", {});

const {AddonTestUtils} = Cu.import("resource://testing-common/AddonTestUtils.jsm", {});

AddonTestUtils.initMochitest(this);

async function createWebExtension(details) {
  let options = {
    manifest: {
      applications: {gecko: {id: details.id}},

      name: details.name,

      permissions: details.permissions,
    },
  };

  if (details.iconURL) {
    options.manifest.icons = {"64": details.iconURL};
  }

  let xpi = AddonTestUtils.createTempWebExtensionFile(options);

  await AddonTestUtils.manuallyInstall(xpi);
}

async function createXULExtension(details) {
  let xpi = AddonTestUtils.createTempXPIFile({
    "install.rdf": {
      id: details.id,
      name: details.name,
      version: "0.1",
      targetApplications: [{
        id: "toolkit@mozilla.org",
        minVersion: "0",
        maxVersion: "*",
      }],
    },
  });

  await AddonTestUtils.manuallyInstall(xpi);
}

let cleanup;

add_task(async function() {
  const DEFAULT_ICON_URL = "chrome://mozapps/skin/extensions/extensionGeneric.svg";

  await SpecialPowers.pushPrefEnv({
    set: [
      ["xpinstall.signatures.required", false],
      ["extensions.autoDisableScopes", 15],
      ["extensions.ui.ignoreUnsigned", true],
    ],
  });

  const ID1 = "addon1@tests.mozilla.org";
  await createWebExtension({
    id: ID1,
    name: "Test 1",
    userDisabled: true,
    permissions: ["history", "https://*/*"],
    iconURL: "foo-icon.png",
  });

  const ID2 = "addon2@tests.mozilla.org";
  await createXULExtension({
    id: ID2,
    name: "Test 2",
  });

  const ID3 = "addon3@tests.mozilla.org";
  await createWebExtension({
    id: ID3,
    name: "Test 3",
    permissions: ["<all_urls>"],
  });

  const ID4 = "addon4@tests.mozilla.org";
  await createWebExtension({
    id: ID4,
    name: "Test 4",
    permissions: ["<all_urls>"],
  });

  testCleanup = async function() {
    // clear out ExtensionsUI state about sideloaded extensions so
    // subsequent tests don't get confused.
    ExtensionsUI.sideloaded.clear();
    ExtensionsUI.emit("change");
  };

  // Navigate away from the starting page to force about:addons to load
  // in a new tab during the tests below.
  gBrowser.selectedBrowser.loadURI("about:robots");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerCleanupFunction(async function() {
    // Return to about:blank when we're done
    gBrowser.selectedBrowser.loadURI("about:blank");
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  });

  hookExtensionsTelemetry();

  let changePromise = new Promise(resolve => {
    ExtensionsUI.on("change", function listener() {
      ExtensionsUI.off("change", listener);
      resolve();
    });
  });
  ExtensionsUI._checkForSideloaded();
  await changePromise;

  // Check for the addons badge on the hamburger menu
  let menuButton = document.getElementById("PanelUI-menu-button");
  is(menuButton.getAttribute("badge-status"), "addon-alert", "Should have addon alert badge");

  // Find the menu entries for sideloaded extensions
  await PanelUI.show();

  let addons = PanelUI.addonNotificationContainer;
  is(addons.children.length, 4, "Have 4 menu entries for sideloaded extensions");

  // Click the first sideloaded extension
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  addons.children[0].click();

  // When we get the permissions prompt, we should be at the extensions
  // list in about:addons
  let panel = await popupPromise;
  is(gBrowser.currentURI.spec, "about:addons", "Foreground tab is at about:addons");

  const VIEW = "addons://list/extension";
  let win = gBrowser.selectedBrowser.contentWindow;
  ok(!win.gViewController.isLoading, "about:addons view is fully loaded");
  is(win.gViewController.currentViewId, VIEW, "about:addons is at extensions list");

  // Check the contents of the notification, then choose "Cancel"
  checkNotification(panel, /\/foo-icon\.png$/, [
    ["webextPerms.hostDescription.allUrls"],
    ["webextPerms.description.history"],
  ]);

  panel.secondaryButton.click();

  let [addon1, addon2, addon3, addon4] = await AddonManager.getAddonsByIDs([ID1, ID2, ID3, ID4]);
  ok(addon1.seen, "Addon should be marked as seen");
  is(addon1.userDisabled, true, "Addon 1 should still be disabled");
  is(addon2.userDisabled, true, "Addon 2 should still be disabled");
  is(addon3.userDisabled, true, "Addon 3 should still be disabled");
  is(addon4.userDisabled, true, "Addon 4 should still be disabled");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Should still have 3 entries in the hamburger menu
  await PanelUI.show();

  addons = PanelUI.addonNotificationContainer;
  is(addons.children.length, 3, "Have 3 menu entries for sideloaded extensions");

  // Click the second sideloaded extension and wait for the notification
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  addons.children[0].click();
  panel = await popupPromise;

  // Again we should be at the extentions list in about:addons
  is(gBrowser.currentURI.spec, "about:addons", "Foreground tab is at about:addons");

  win = gBrowser.selectedBrowser.contentWindow;
  ok(!win.gViewController.isLoading, "about:addons view is fully loaded");
  is(win.gViewController.currentViewId, VIEW, "about:addons is at extensions list");

  // Check the notification contents.
  checkNotification(panel, DEFAULT_ICON_URL, []);

  // This time accept the install.
  panel.button.click();

  [addon1, addon2, addon3, addon4] = await AddonManager.getAddonsByIDs([ID1, ID2, ID3, ID4]);
  is(addon1.userDisabled, true, "Addon 1 should still be disabled");
  is(addon2.userDisabled, false, "Addon 2 should now be enabled");
  is(addon3.userDisabled, true, "Addon 3 should still be disabled");
  is(addon4.userDisabled, true, "Addon 4 should still be disabled");

  // Should still have 2 entries in the hamburger menu
  await PanelUI.show();

  addons = PanelUI.addonNotificationContainer;
  is(addons.children.length, 2, "Have 2 menu entries for sideloaded extensions");

  // Close the hamburger menu and go directly to the addons manager
  await PanelUI.hide();

  win = await BrowserOpenAddonsMgr(VIEW);

  let list = win.document.getElementById("addon-list");

  // Make sure XBL bindings are applied
  list.clientHeight;

  let item = list.children.find(_item => _item.value == ID3);
  ok(item, "Found entry for sideloaded extension in about:addons");
  item.scrollIntoView({behavior: "instant"});

  ok(is_visible(item._enableBtn), "Enable button is visible for sideloaded extension");
  ok(is_hidden(item._disableBtn), "Disable button is not visible for sideloaded extension");

  // When clicking enable we should see the permissions notification
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  BrowserTestUtils.synthesizeMouseAtCenter(item._enableBtn, {},
                                           gBrowser.selectedBrowser);
  panel = await popupPromise;
  checkNotification(panel, DEFAULT_ICON_URL, [["webextPerms.hostDescription.allUrls"]]);

  // Accept the permissions
  panel.button.click();

  addon3 = await AddonManager.getAddonByID(ID3);
  is(addon3.userDisabled, false, "Addon 3 should be enabled");

  // Should still have 1 entry in the hamburger menu
  await PanelUI.show();

  addons = PanelUI.addonNotificationContainer;
  is(addons.children.length, 1, "Have 1 menu entry for sideloaded extensions");

  // Close the hamburger menu and go to the detail page for this addon
  await PanelUI.hide();

  win = await BrowserOpenAddonsMgr(`addons://detail/${encodeURIComponent(ID4)}`);
  let button = win.document.getElementById("detail-enable-btn");

  // When clicking enable we should see the permissions notification
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  BrowserTestUtils.synthesizeMouseAtCenter(button, {},
                                           gBrowser.selectedBrowser);
  panel = await popupPromise;
  checkNotification(panel, DEFAULT_ICON_URL, [["webextPerms.hostDescription.allUrls"]]);

  // Accept the permissions
  panel.button.click();

  addon4 = await AddonManager.getAddonByID(ID4);
  is(addon4.userDisabled, false, "Addon 4 should be enabled");

  // We should have recorded 1 cancelled followed by 3 accepted sideloads.
  expectTelemetry(["sideloadRejected", "sideloadAccepted", "sideloadAccepted", "sideloadAccepted"]);

  isnot(menuButton.getAttribute("badge-status"), "addon-alert", "Should no longer have addon alert badge");

  await new Promise(resolve => setTimeout(resolve, 100));

  for (let addon of [addon1, addon2, addon3, addon4]) {
    addon.uninstall();
  }

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
