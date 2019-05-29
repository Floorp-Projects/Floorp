/* eslint-disable mozilla/no-arbitrary-setTimeout */
const {AddonManagerPrivate} = ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

AddonTestUtils.initMochitest(this);

hookExtensionsTelemetry();
AddonTestUtils.hookAMTelemetryEvents();

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

function promiseEvent(eventEmitter, event) {
  return new Promise(resolve => {
    eventEmitter.once(event, resolve);
  });
}

async function getAddonElement(managerWindow, addonId) {
  if (managerWindow.useHtmlViews) {
    // about:addons is using the new HTML page.
    const {contentDocument: doc} = managerWindow.document.getElementById("html-view-browser");
    const card = await BrowserTestUtils.waitForCondition(() =>
      doc.querySelector(`addon-card[addon-id="${addonId}"]`),
      `Found entry for sideload extension addon "${addonId}" in HTML about:addons`);

    return card;
  }

  // about:addons is using the XUL-based views.
  let list = managerWindow.document.getElementById("addon-list");
  // Make sure XBL bindings are applied
  list.clientHeight;
  const item = Array.from(list.children).find(_item => _item.value == addonId);
  ok(item, "Found entry for sideloaded extension in about:addons");

  return item;
}

function assertDisabledSideloadedAddonElement(managerWindow, addonElement) {
  if (managerWindow.useHtmlViews) {
    // about:addons is using the new HTML page.
    const doc = addonElement.ownerDocument;
    const enableBtn = addonElement.querySelector('[action="toggle-disabled"]');
    is(doc.l10n.getAttributes(enableBtn).id, "enable-addon-button",
       "The button has the enable label");
  } else {
    addonElement.scrollIntoView({behavior: "instant"});
    ok(BrowserTestUtils.is_visible(addonElement._enableBtn),
       "Enable button is visible for sideloaded extension");
    ok(BrowserTestUtils.is_hidden(addonElement._disableBtn),
       "Disable button is not visible for sideloaded extension");
   }
}

function clickEnableExtension(managerWindow, addonElement) {
  if (managerWindow.useHtmlViews) {
    addonElement.querySelector('[action="toggle-disabled"]').click();
  } else {
    BrowserTestUtils.synthesizeMouseAtCenter(addonElement._enableBtn, {},
                                             gBrowser.selectedBrowser);
  }
}

async function test_sideloading({useHtmlViews}) {
  const DEFAULT_ICON_URL = "chrome://mozapps/skin/extensions/extensionGeneric.svg";

  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.htmlaboutaddons.enabled", useHtmlViews],
      ["xpinstall.signatures.required", false],
      ["extensions.autoDisableScopes", 15],
      ["extensions.ui.ignoreUnsigned", true],
      ["extensions.allowPrivateBrowsingByDefault", false],
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
  await createWebExtension({
    id: ID2,
    name: "Test 2",
    permissions: ["<all_urls>"],
  });

  const ID3 = "addon3@tests.mozilla.org";
  await createWebExtension({
    id: ID3,
    name: "Test 3",
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
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:robots");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerCleanupFunction(async function() {
    // Return to about:blank when we're done
    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:blank");
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  });


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
  await gCUITestUtils.openMainMenu();

  let addons = PanelUI.addonNotificationContainer;
  is(addons.children.length, 3, "Have 3 menu entries for sideloaded extensions");

  info("Test disabling sideloaded addon 1 using the permission prompt secondary button");

  // Click the first sideloaded extension
  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  addons.children[0].click();

  // The click should hide the main menu. This is currently synchronous.
  ok(PanelUI.panel.state != "open", "Main menu is closed or closing.");

  // When we get the permissions prompt, we should be at the extensions
  // list in about:addons
  let panel = await popupPromise;
  is(gBrowser.currentURI.spec, "about:addons", "Foreground tab is at about:addons");

  const VIEW = "addons://list/extension";
  let win = gBrowser.selectedBrowser.contentWindow;

  await BrowserTestUtils.waitForCondition(() => !win.gViewController.isLoading,
                                          "about:addons view is fully loaded");
  is(win.gViewController.currentViewId, VIEW, "about:addons is at extensions list");

  // Check the contents of the notification, then choose "Cancel"
  checkNotification(panel, /\/foo-icon\.png$/, [
    ["webextPerms.hostDescription.allUrls"],
    ["webextPerms.description.history"],
  ]);

  panel.secondaryButton.click();

  let [addon1, addon2, addon3] = await AddonManager.getAddonsByIDs([ID1, ID2, ID3]);
  ok(addon1.seen, "Addon should be marked as seen");
  is(addon1.userDisabled, true, "Addon 1 should still be disabled");
  is(addon2.userDisabled, true, "Addon 2 should still be disabled");
  is(addon3.userDisabled, true, "Addon 3 should still be disabled");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Should still have 2 entries in the hamburger menu
  await gCUITestUtils.openMainMenu();

  addons = PanelUI.addonNotificationContainer;
  is(addons.children.length, 2, "Have 2 menu entries for sideloaded extensions");

  // Close the hamburger menu and go directly to the addons manager
  await gCUITestUtils.hideMainMenu();

  win = await BrowserOpenAddonsMgr(VIEW);

  if (win.gViewController.isLoading) {
    await new Promise(resolve => win.document.addEventListener("ViewChanged", resolve, {once: true}));
  }

  // XUL or HTML about:addons addon entry element.
  const addonElement = await getAddonElement(win, ID2);

  assertDisabledSideloadedAddonElement(win, addonElement);

  info("Test enabling sideloaded addon 2 from about:addons enable button");

  // When clicking enable we should see the permissions notification
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  clickEnableExtension(win, addonElement);
  panel = await popupPromise;
  checkNotification(panel, DEFAULT_ICON_URL, [["webextPerms.hostDescription.allUrls"]]);

  // Test incognito checkbox in post install notification
  function setupPostInstallNotificationTest() {
    let promiseNotificationShown = promiseAppMenuNotificationShown("addon-installed");
    return async function(addon) {
      info(`Expect post install notification for "${addon.name}"`);
      let postInstallPanel = await promiseNotificationShown;
      let incognitoCheckbox = postInstallPanel.querySelector("#addon-incognito-checkbox");
      is(window.AppMenuNotifications.activeNotification.options.name, addon.name,
         "Got the expected addon name in the active notification");
      ok(incognitoCheckbox, "Got an incognito checkbox in the post install notification panel");
      ok(!incognitoCheckbox.hidden, "Incognito checkbox should not be hidden");
      // Dismiss post install notification.
      postInstallPanel.button.click();
    };
  }

  // Setup async test for post install notification on addon 2
  let testPostInstallIncognitoCheckbox = setupPostInstallNotificationTest();

  // Accept the permissions
  panel.button.click();
  await promiseEvent(ExtensionsUI, "change");

  addon2 = await AddonManager.getAddonByID(ID2);
  is(addon2.userDisabled, false, "Addon 2 should be enabled");

  // Test post install notification on addon 2.
  await testPostInstallIncognitoCheckbox(addon2);

  // Should still have 1 entry in the hamburger menu
  await gCUITestUtils.openMainMenu();

  addons = PanelUI.addonNotificationContainer;
  is(addons.children.length, 1, "Have 1 menu entry for sideloaded extensions");

  // Close the hamburger menu and go to the detail page for this addon
  await gCUITestUtils.hideMainMenu();

  win = await BrowserOpenAddonsMgr(`addons://detail/${encodeURIComponent(ID3)}`);

  info("Test enabling sideloaded addon 3 from app menu");
  // Trigger addon 3 install as triggered from the app menu, to be able to cover the
  // post install notification that should be triggered when the permission
  // dialog is accepted from that flow.
  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  ExtensionsUI.showSideloaded(gBrowser, addon3);

  panel = await popupPromise;
  checkNotification(panel, DEFAULT_ICON_URL, [["webextPerms.hostDescription.allUrls"]]);

  // Setup async test for post install notification on addon 3
  testPostInstallIncognitoCheckbox = setupPostInstallNotificationTest();

  // Accept the permissions
  panel.button.click();
  await promiseEvent(ExtensionsUI, "change");

  addon3 = await AddonManager.getAddonByID(ID3);
  is(addon3.userDisabled, false, "Addon 3 should be enabled");

  // Test post install notification on addon 3.
  await testPostInstallIncognitoCheckbox(addon3);

  // We should have recorded 1 cancelled followed by 2 accepted sideloads.
  expectTelemetry(["sideloadRejected", "sideloadAccepted", "sideloadAccepted"]);

  isnot(menuButton.getAttribute("badge-status"), "addon-alert", "Should no longer have addon alert badge");

  await new Promise(resolve => setTimeout(resolve, 100));

  for (let addon of [addon1, addon2, addon3]) {
    await addon.uninstall();
  }

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Assert that the expected AddonManager telemetry are being recorded.
  const expectedExtra = {source: "app-profile", method: "sideload"};

  const baseEvent = {object: "extension", extra: expectedExtra};
  const createBaseEventAddon = (n) => ({...baseEvent, value: `addon${n}@tests.mozilla.org`});
  const getEventsForAddonId = (events, addonId) => events.filter(ev => ev.value === addonId);

  const amEvents = AddonTestUtils.getAMTelemetryEvents();

  // Test telemetry events for addon1 (1 permission and 1 origin).
  info("Test telemetry events collected for addon1");

  const baseEventAddon1 = createBaseEventAddon(1);
  const collectedEventsAddon1 = getEventsForAddonId(amEvents, baseEventAddon1.value);
  const expectedEventsAddon1 = [
    {
      ...baseEventAddon1, method: "sideload_prompt",
      extra: {...expectedExtra, num_strings: "2"},
    },
    {...baseEventAddon1, method: "uninstall"},
  ];

  let i = 0;
  for (let event of collectedEventsAddon1) {
    Assert.deepEqual(event, expectedEventsAddon1[i++],
                     "Got the expected telemetry event");
  }

  is(collectedEventsAddon1.length, expectedEventsAddon1.length,
     "Got the expected number of telemetry events for addon1");

  const baseEventAddon2 = createBaseEventAddon(2);
  const collectedEventsAddon2 = getEventsForAddonId(amEvents, baseEventAddon2.value);
  const expectedEventsAddon2 = [
    {
      ...baseEventAddon2, method: "sideload_prompt",
      extra: {...expectedExtra, num_strings: "1"},
    },
    {...baseEventAddon2, method: "enable"},
    {...baseEventAddon2, method: "uninstall"},
  ];

  i = 0;
  for (let event of collectedEventsAddon2) {
    Assert.deepEqual(event, expectedEventsAddon2[i++],
                     "Got the expected telemetry event");
  }

  is(collectedEventsAddon2.length, expectedEventsAddon2.length,
     "Got the expected number of telemetry events for addon2");
}

add_task(async function test_xul_aboutaddons_sideloading() {
  await test_sideloading({useHtmlViews: false});
});

add_task(async function test_html_aboutaddons_sideloading() {
  await test_sideloading({useHtmlViews: true});
});
