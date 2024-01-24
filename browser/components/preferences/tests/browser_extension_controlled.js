/* eslint-env webextensions */

const PROXY_PREF = "network.proxy.type";
const HOMEPAGE_URL_PREF = "browser.startup.homepage";
const HOMEPAGE_OVERRIDE_KEY = "homepage_override";
const URL_OVERRIDES_TYPE = "url_overrides";
const NEW_TAB_KEY = "newTabURL";
const PREF_SETTING_TYPE = "prefs";

ChromeUtils.defineESModuleGetters(this, {
  AboutNewTab: "resource:///modules/AboutNewTab.sys.mjs",
  ExtensionSettingsStore:
    "resource://gre/modules/ExtensionSettingsStore.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(this, "proxyType", PROXY_PREF);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
AddonTestUtils.initMochitest(this);

const { ExtensionPreferencesManager } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPreferencesManager.sys.mjs"
);

const TEST_DIR = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
const CHROME_URL_ROOT = TEST_DIR + "/";
const PERMISSIONS_URL =
  "chrome://browser/content/preferences/dialogs/sitePermissions.xhtml";
let sitePermissionsDialog;

function getSupportsFile(path) {
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
    Ci.nsIChromeRegistry
  );
  let uri = Services.io.newURI(CHROME_URL_ROOT + path);
  let fileurl = cr.convertChromeURL(uri);
  return fileurl.QueryInterface(Ci.nsIFileURL);
}

function waitForMessageChange(
  element,
  cb,
  opts = { attributes: true, attributeFilter: ["hidden"] }
) {
  return waitForMutation(element, opts, cb);
}

function getElement(id, doc = gBrowser.contentDocument) {
  return doc.getElementById(id);
}

function waitForMessageHidden(messageId, doc) {
  return waitForMessageChange(
    getElement(messageId, doc),
    target => target.hidden
  );
}

function waitForMessageShown(messageId, doc) {
  return waitForMessageChange(
    getElement(messageId, doc),
    target => !target.hidden
  );
}

function waitForEnableMessage(messageId, doc) {
  return waitForMessageChange(
    getElement(messageId, doc),
    target => target.classList.contains("extension-controlled-disabled"),
    { attributeFilter: ["class"], attributes: true }
  );
}

function waitForMessageContent(messageId, l10nId, doc) {
  return waitForMessageChange(
    getElement(messageId, doc),
    target => doc.l10n.getAttributes(target).id === l10nId,
    { childList: true }
  );
}

async function openNotificationsPermissionDialog() {
  let dialogOpened = promiseLoadSubDialog(PERMISSIONS_URL);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    let doc = content.document;
    let settingsButton = doc.getElementById("notificationSettingsButton");
    settingsButton.click();
  });

  sitePermissionsDialog = await dialogOpened;
  await sitePermissionsDialog.document.mozSubdialogReady;
}

async function disableExtensionViaClick(labelId, disableButtonId, doc) {
  let controlledLabel = doc.getElementById(labelId);

  let enableMessageShown = waitForEnableMessage(labelId, doc);
  doc.getElementById(disableButtonId).click();
  await enableMessageShown;

  let controlledDescription = controlledLabel.querySelector("description");
  is(
    doc.l10n.getAttributes(controlledDescription.querySelector("label")).id,
    "extension-controlled-enable",
    "The user is notified of how to enable the extension again."
  );

  // The user can dismiss the enable instructions.
  let hidden = waitForMessageHidden(labelId, doc);
  controlledLabel.querySelector("image:last-of-type").click();
  await hidden;
}

async function reEnableExtension(addon, labelId) {
  let controlledMessageShown = waitForMessageShown(labelId);
  await addon.enable();
  await controlledMessageShown;
}

add_task(async function testExtensionControlledHomepage() {
  const ADDON_ID = "@set_homepage";
  const SECOND_ADDON_ID = "@second_set_homepage";

  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });
  let homepagePref = () => Services.prefs.getCharPref(HOMEPAGE_URL_PREF);
  let originalHomepagePref = homepagePref();
  is(
    gBrowser.currentURI.spec,
    "about:preferences#home",
    "#home should be in the URI for about:preferences"
  );
  let doc = gBrowser.contentDocument;
  let homeModeEl = doc.getElementById("homeMode");
  let customSettingsSection = doc.getElementById("customSettings");

  is(homeModeEl.itemCount, 3, "The menu list starts with 3 options");

  let promise = TestUtils.waitForCondition(
    () => homeModeEl.itemCount === 4,
    "wait for the addon option to be added as an option in the menu list"
  );
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      name: "set_homepage",
      browser_specific_settings: {
        gecko: {
          id: ADDON_ID,
        },
      },
      chrome_settings_overrides: { homepage: "/home.html" },
    },
  });
  await extension.startup();
  await promise;

  // The homepage is set to the default and the custom settings section is hidden
  is(homeModeEl.disabled, false, "The homepage menulist is enabled");
  is(
    customSettingsSection.hidden,
    true,
    "The custom settings element is hidden"
  );

  let addon = await AddonManager.getAddonByID(ADDON_ID);
  is(
    homeModeEl.value,
    addon.id,
    "the home select menu's value is set to the addon"
  );

  promise = TestUtils.waitForPrefChange(HOMEPAGE_URL_PREF);
  // Set the Menu to the default value
  homeModeEl.value = "0";
  homeModeEl.dispatchEvent(new Event("command"));
  await promise;
  is(homepagePref(), originalHomepagePref, "homepage is set back to default");
  let levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    addon.id,
    HOMEPAGE_OVERRIDE_KEY,
    PREF_SETTING_TYPE
  );
  is(
    levelOfControl,
    "not_controllable",
    "getLevelOfControl returns not_controllable."
  );
  let setting = await ExtensionPreferencesManager.getSetting(
    HOMEPAGE_OVERRIDE_KEY
  );
  ok(!setting.value, "the setting is not set.");

  promise = TestUtils.waitForPrefChange(HOMEPAGE_URL_PREF);
  // Set the menu to the addon value
  homeModeEl.value = ADDON_ID;
  homeModeEl.dispatchEvent(new Event("command"));
  await promise;
  ok(
    homepagePref().startsWith("moz-extension") &&
      homepagePref().endsWith("home.html"),
    "Home url should be provided by the extension."
  );
  levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    addon.id,
    HOMEPAGE_OVERRIDE_KEY,
    PREF_SETTING_TYPE
  );
  is(
    levelOfControl,
    "controlled_by_this_extension",
    "getLevelOfControl returns controlled_by_this_extension."
  );
  setting = await ExtensionPreferencesManager.getSetting(HOMEPAGE_OVERRIDE_KEY);
  ok(
    setting.value.startsWith("moz-extension") &&
      setting.value.endsWith("home.html"),
    "The setting value is the same as the extension."
  );

  // Add a second extension, ensure it is added to the menulist and selected.
  promise = TestUtils.waitForCondition(
    () => homeModeEl.itemCount == 5,
    "addon option is added as an option in the menu list"
  );
  let secondExtension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      name: "second_set_homepage",
      browser_specific_settings: {
        gecko: {
          id: SECOND_ADDON_ID,
        },
      },
      chrome_settings_overrides: { homepage: "/home2.html" },
    },
  });
  await secondExtension.startup();
  await promise;

  let secondAddon = await AddonManager.getAddonByID(SECOND_ADDON_ID);
  is(homeModeEl.value, SECOND_ADDON_ID, "home menulist is set to the add-on");

  levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    secondAddon.id,
    HOMEPAGE_OVERRIDE_KEY,
    PREF_SETTING_TYPE
  );
  is(
    levelOfControl,
    "controlled_by_this_extension",
    "getLevelOfControl returns controlled_by_this_extension."
  );
  setting = await ExtensionPreferencesManager.getSetting(HOMEPAGE_OVERRIDE_KEY);
  ok(
    setting.value.startsWith("moz-extension") &&
      setting.value.endsWith("home2.html"),
    "The setting value is the same as the extension."
  );

  promise = TestUtils.waitForCondition(
    () => homeModeEl.itemCount == 4,
    "addon option is no longer an option in the menu list after disable, even if it was not selected"
  );
  await addon.disable();
  await promise;

  // Ensure that re-enabling an addon adds it back to the menulist
  promise = TestUtils.waitForCondition(
    () => homeModeEl.itemCount == 5,
    "addon option is added again to the menulist when enabled"
  );
  await addon.enable();
  await promise;

  promise = TestUtils.waitForCondition(
    () => homeModeEl.itemCount == 4,
    "addon option is no longer an option in the menu list after disable"
  );
  await secondAddon.disable();
  await promise;

  promise = TestUtils.waitForCondition(
    () => homeModeEl.itemCount == 5,
    "addon option is added again to the menulist when enabled"
  );
  await secondAddon.enable();
  await promise;

  promise = TestUtils.waitForCondition(
    () => homeModeEl.itemCount == 3,
    "addon options are no longer an option in the menu list after disabling all addons"
  );
  await secondAddon.disable();
  await addon.disable();
  await promise;

  is(homeModeEl.value, "0", "addon option is not selected in the menu list");

  levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    secondAddon.id,
    HOMEPAGE_OVERRIDE_KEY,
    PREF_SETTING_TYPE
  );
  is(
    levelOfControl,
    "controllable_by_this_extension",
    "getLevelOfControl returns controllable_by_this_extension."
  );
  setting = await ExtensionPreferencesManager.getSetting(HOMEPAGE_OVERRIDE_KEY);
  ok(!setting.value, "The setting value is back to default.");

  // The homepage elements are reset to their original state.
  is(homepagePref(), originalHomepagePref, "homepage is set back to default");
  is(homeModeEl.disabled, false, "The homepage menulist is enabled");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await extension.unload();
  await secondExtension.unload();
});

add_task(async function testPrefLockedHomepage() {
  const ADDON_ID = "@set_homepage";
  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });
  let doc = gBrowser.contentDocument;
  is(
    gBrowser.currentURI.spec,
    "about:preferences#home",
    "#home should be in the URI for about:preferences"
  );

  let homePagePref = "browser.startup.homepage";
  let buttonPrefs = [
    "pref.browser.homepage.disable_button.current_page",
    "pref.browser.homepage.disable_button.bookmark_page",
    "pref.browser.homepage.disable_button.restore_default",
  ];
  let homeModeEl = doc.getElementById("homeMode");
  let homePageInput = doc.getElementById("homePageUrl");
  let prefs = Services.prefs.getDefaultBranch(null);
  let mutationOpts = { attributes: true, attributeFilter: ["disabled"] };

  // Helper functions.
  let getButton = pref =>
    doc.querySelector(`.homepage-button[preference="${pref}"`);
  let waitForAllMutations = () =>
    Promise.all(
      buttonPrefs
        .map(pref => waitForMutation(getButton(pref), mutationOpts))
        .concat([
          waitForMutation(homeModeEl, mutationOpts),
          waitForMutation(homePageInput, mutationOpts),
        ])
    );
  let getHomepage = () =>
    Services.prefs.getCharPref("browser.startup.homepage");

  let originalHomepage = getHomepage();
  let extensionHomepage = "https://developer.mozilla.org/";
  let lockedHomepage = "http://www.yahoo.com";

  let lockPrefs = () => {
    buttonPrefs.forEach(pref => {
      prefs.setBoolPref(pref, true);
      prefs.lockPref(pref);
    });
    // Do the homepage last since that's the only pref that triggers a UI update.
    prefs.setCharPref(homePagePref, lockedHomepage);
    prefs.lockPref(homePagePref);
  };
  let unlockPrefs = () => {
    buttonPrefs.forEach(pref => {
      prefs.unlockPref(pref);
      prefs.setBoolPref(pref, false);
    });
    // Do the homepage last since that's the only pref that triggers a UI update.
    prefs.unlockPref(homePagePref);
    prefs.setCharPref(homePagePref, originalHomepage);
  };

  // Lock or unlock prefs then wait for all mutations to finish.
  // Expects a bool indicating if we should lock or unlock.
  let waitForLockMutations = lock => {
    let mutationsDone = waitForAllMutations();
    if (lock) {
      lockPrefs();
    } else {
      unlockPrefs();
    }
    return mutationsDone;
  };

  Assert.notEqual(
    originalHomepage,
    extensionHomepage,
    "The extension will change the homepage"
  );

  // Install an extension that sets the homepage to MDN.
  let promise = TestUtils.waitForPrefChange(HOMEPAGE_URL_PREF);
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      name: "set_homepage",
      browser_specific_settings: {
        gecko: {
          id: ADDON_ID,
        },
      },
      chrome_settings_overrides: { homepage: "https://developer.mozilla.org/" },
    },
  });
  await extension.startup();
  await promise;

  // Check that everything is still disabled, homepage didn't change.
  is(
    getHomepage(),
    extensionHomepage,
    "The reported homepage is set by the extension"
  );
  is(
    homePageInput.value,
    extensionHomepage,
    "The homepage is set by the extension"
  );

  // Lock all of the prefs, wait for the UI to update.
  await waitForLockMutations(true);

  // Check that everything is now disabled.
  is(getHomepage(), lockedHomepage, "The reported homepage is set by the pref");
  is(homePageInput.value, lockedHomepage, "The homepage is set by the pref");
  is(
    homePageInput.disabled,
    true,
    "The homepage is disabed when the pref is locked"
  );

  buttonPrefs.forEach(pref => {
    is(
      getButton(pref).disabled,
      true,
      `The ${pref} button is disabled when locked`
    );
  });

  let levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    ADDON_ID,
    HOMEPAGE_OVERRIDE_KEY,
    PREF_SETTING_TYPE
  );
  is(
    levelOfControl,
    "not_controllable",
    "getLevelOfControl returns not_controllable, the pref is locked."
  );

  // Verify that the UI is selecting the extension's Id in the menulist
  let unlockedPromise = TestUtils.waitForCondition(
    () => homeModeEl.value == ADDON_ID,
    "Homepage menulist value is equal to the extension ID"
  );
  // Unlock the prefs, wait for the UI to update.
  unlockPrefs();
  await unlockedPromise;

  is(
    homeModeEl.disabled,
    false,
    "the home select element is not disabled when the pref is not locked"
  );
  is(
    homePageInput.disabled,
    false,
    "The homepage is enabled when the pref is unlocked"
  );
  is(
    getHomepage(),
    extensionHomepage,
    "The homepage is reset to extension page"
  );

  levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    ADDON_ID,
    HOMEPAGE_OVERRIDE_KEY,
    PREF_SETTING_TYPE
  );
  is(
    levelOfControl,
    "controlled_by_this_extension",
    "getLevelOfControl returns controlled_by_this_extension after prefs are unlocked."
  );
  let setting = await ExtensionPreferencesManager.getSetting(
    HOMEPAGE_OVERRIDE_KEY
  );
  is(
    setting.value,
    extensionHomepage,
    "The setting value is equal to the extensionHomepage."
  );

  // Uninstall the add-on.
  promise = TestUtils.waitForPrefChange(HOMEPAGE_URL_PREF);
  await extension.unload();
  await promise;

  setting = await ExtensionPreferencesManager.getSetting(HOMEPAGE_OVERRIDE_KEY);
  ok(!setting, "The setting is gone after the addon is uninstalled.");

  // Check that everything is now enabled again.
  is(
    getHomepage(),
    originalHomepage,
    "The reported homepage is reset to original value"
  );
  is(homePageInput.value, "", "The homepage is empty");
  is(
    homePageInput.disabled,
    false,
    "The homepage is enabled after clearing lock"
  );
  is(
    homeModeEl.disabled,
    false,
    "Homepage menulist is enabled after clearing lock"
  );
  buttonPrefs.forEach(pref => {
    is(
      getButton(pref).disabled,
      false,
      `The ${pref} button is enabled when unlocked`
    );
  });

  // Lock the prefs without an extension.
  await waitForLockMutations(true);

  // Check that everything is now disabled.
  is(getHomepage(), lockedHomepage, "The reported homepage is set by the pref");
  is(homePageInput.value, lockedHomepage, "The homepage is set by the pref");
  is(
    homePageInput.disabled,
    true,
    "The homepage is disabed when the pref is locked"
  );
  is(
    homeModeEl.disabled,
    true,
    "Homepage menulist is disabled when pref is locked"
  );
  buttonPrefs.forEach(pref => {
    is(
      getButton(pref).disabled,
      true,
      `The ${pref} button is disabled when locked`
    );
  });

  // Unlock the prefs without an extension.
  await waitForLockMutations(false);

  // Check that everything is enabled again.
  is(
    getHomepage(),
    originalHomepage,
    "The homepage is reset to the original value"
  );
  is(homePageInput.value, "", "The homepage is clear after being unlocked");
  is(
    homePageInput.disabled,
    false,
    "The homepage is enabled after clearing lock"
  );
  is(
    homeModeEl.disabled,
    false,
    "Homepage menulist is enabled after clearing lock"
  );
  buttonPrefs.forEach(pref => {
    is(
      getButton(pref).disabled,
      false,
      `The ${pref} button is enabled when unlocked`
    );
  });

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testExtensionControlledNewTab() {
  const ADDON_ID = "@set_newtab";
  const SECOND_ADDON_ID = "@second_set_newtab";
  const DEFAULT_NEWTAB = "about:newtab";
  const NEWTAB_CONTROLLED_PREF = "browser.newtab.extensionControlled";

  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });
  is(
    gBrowser.currentURI.spec,
    "about:preferences#home",
    "#home should be in the URI for about:preferences"
  );

  let doc = gBrowser.contentDocument;
  let newTabMenuList = doc.getElementById("newTabMode");
  // The new tab page is set to the default.
  is(AboutNewTab.newTabURL, DEFAULT_NEWTAB, "new tab is set to default");

  let promise = TestUtils.waitForCondition(
    () => newTabMenuList.itemCount == 3,
    "addon option is added as an option in the menu list"
  );
  // Install an extension that will set the new tab page.
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      name: "set_newtab",
      browser_specific_settings: {
        gecko: {
          id: ADDON_ID,
        },
      },
      chrome_url_overrides: { newtab: "/newtab.html" },
    },
  });
  await extension.startup();

  await promise;
  let addon = await AddonManager.getAddonByID(ADDON_ID);

  is(newTabMenuList.value, ADDON_ID, "New tab menulist is set to the add-on");

  let levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    addon.id,
    NEW_TAB_KEY,
    URL_OVERRIDES_TYPE
  );
  is(
    levelOfControl,
    "controlled_by_this_extension",
    "getLevelOfControl returns controlled_by_this_extension."
  );
  let setting = ExtensionSettingsStore.getSetting(
    URL_OVERRIDES_TYPE,
    NEW_TAB_KEY
  );
  ok(
    setting.value.startsWith("moz-extension") &&
      setting.value.endsWith("newtab.html"),
    "The url_overrides is set by this extension"
  );

  promise = TestUtils.waitForPrefChange(NEWTAB_CONTROLLED_PREF);
  // Set the menu to the default value
  newTabMenuList.value = "0";
  newTabMenuList.dispatchEvent(new Event("command"));
  await promise;
  let newTabPref = Services.prefs.getBoolPref(NEWTAB_CONTROLLED_PREF, false);
  is(newTabPref, false, "the new tab is not controlled");

  levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    addon.id,
    NEW_TAB_KEY,
    URL_OVERRIDES_TYPE
  );
  is(
    levelOfControl,
    "not_controllable",
    "getLevelOfControl returns not_controllable."
  );
  setting = ExtensionSettingsStore.getSetting(URL_OVERRIDES_TYPE, NEW_TAB_KEY);
  ok(!setting.value, "The url_overrides is not set by this extension");

  promise = TestUtils.waitForPrefChange(NEWTAB_CONTROLLED_PREF);
  // Set the menu to a the addon value
  newTabMenuList.value = ADDON_ID;
  newTabMenuList.dispatchEvent(new Event("command"));
  await promise;
  newTabPref = Services.prefs.getBoolPref(NEWTAB_CONTROLLED_PREF, false);
  is(newTabPref, true, "the new tab is controlled");

  // Add a second extension, ensure it is added to the menulist and selected.
  promise = TestUtils.waitForCondition(
    () => newTabMenuList.itemCount == 4,
    "addon option is added as an option in the menu list"
  );
  let secondExtension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      name: "second_set_newtab",
      browser_specific_settings: {
        gecko: {
          id: SECOND_ADDON_ID,
        },
      },
      chrome_url_overrides: { newtab: "/newtab2.html" },
    },
  });
  await secondExtension.startup();
  await promise;
  let secondAddon = await AddonManager.getAddonByID(SECOND_ADDON_ID);
  is(
    newTabMenuList.value,
    SECOND_ADDON_ID,
    "New tab menulist is set to the add-on"
  );

  levelOfControl = await ExtensionPreferencesManager.getLevelOfControl(
    secondAddon.id,
    NEW_TAB_KEY,
    URL_OVERRIDES_TYPE
  );
  is(
    levelOfControl,
    "controlled_by_this_extension",
    "getLevelOfControl returns controlled_by_this_extension."
  );
  setting = ExtensionSettingsStore.getSetting(URL_OVERRIDES_TYPE, NEW_TAB_KEY);
  ok(
    setting.value.startsWith("moz-extension") &&
      setting.value.endsWith("newtab2.html"),
    "The url_overrides is set by the second extension"
  );

  promise = TestUtils.waitForCondition(
    () => newTabMenuList.itemCount == 3,
    "addon option is no longer an option in the menu list after disable, even if it was not selected"
  );
  await addon.disable();
  await promise;

  // Ensure that re-enabling an addon adds it back to the menulist
  promise = TestUtils.waitForCondition(
    () => newTabMenuList.itemCount == 4,
    "addon option is added again to the menulist when enabled"
  );
  await addon.enable();
  await promise;

  promise = TestUtils.waitForCondition(
    () => newTabMenuList.itemCount == 3,
    "addon option is no longer an option in the menu list after disable"
  );
  await secondAddon.disable();
  await promise;

  promise = TestUtils.waitForCondition(
    () => newTabMenuList.itemCount == 4,
    "addon option is added again to the menulist when enabled"
  );
  await secondAddon.enable();
  await promise;

  promise = TestUtils.waitForCondition(
    () => newTabMenuList.itemCount == 2,
    "addon options are all removed after disabling all"
  );
  await addon.disable();
  await secondAddon.disable();
  await promise;
  is(
    AboutNewTab.newTabURL,
    DEFAULT_NEWTAB,
    "new tab page is set back to default"
  );

  // Cleanup the tabs and add-on.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await extension.unload();
  await secondExtension.unload();
});

add_task(async function testExtensionControlledWebNotificationsPermission() {
  let manifest = {
    manifest_version: 2,
    name: "TestExtension",
    version: "1.0",
    description: "Testing WebNotificationsDisable",
    browser_specific_settings: { gecko: { id: "@web_notifications_disable" } },
    permissions: ["browserSettings"],
    browser_action: {
      default_title: "Testing",
    },
  };

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await openNotificationsPermissionDialog();

  let doc = sitePermissionsDialog.document;
  let extensionControlledContent = doc.getElementById(
    "browserNotificationsPermissionExtensionContent"
  );

  // Test that extension content is initially hidden.
  ok(
    extensionControlledContent.hidden,
    "Extension content is initially hidden"
  );

  // Install an extension that will disable web notifications permission.
  let messageShown = waitForMessageShown(
    "browserNotificationsPermissionExtensionContent",
    doc
  );
  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "permanent",
    background() {
      browser.browserSettings.webNotificationsDisabled.set({ value: true });
      browser.test.sendMessage("load-extension");
    },
  });
  await extension.startup();
  await extension.awaitMessage("load-extension");
  await messageShown;

  let controlledDesc = extensionControlledContent.querySelector("description");
  Assert.deepEqual(
    doc.l10n.getAttributes(controlledDesc),
    {
      id: "extension-controlling-web-notifications",
      args: {
        name: "TestExtension",
      },
    },
    "The user is notified that an extension is controlling the web notifications permission"
  );
  is(
    extensionControlledContent.hidden,
    false,
    "The extension controlled row is not hidden"
  );

  // Disable the extension.
  doc.getElementById("disableNotificationsPermissionExtension").click();

  // Verify the user is notified how to enable the extension.
  await waitForEnableMessage(extensionControlledContent.id, doc);
  is(
    doc.l10n.getAttributes(controlledDesc.querySelector("label")).id,
    "extension-controlled-enable",
    "The user is notified of how to enable the extension again"
  );

  // Verify the enable message can be dismissed.
  let hidden = waitForMessageHidden(extensionControlledContent.id, doc);
  let dismissButton = controlledDesc.querySelector("image:last-of-type");
  dismissButton.click();
  await hidden;

  // Verify that the extension controlled content in hidden again.
  is(
    extensionControlledContent.hidden,
    true,
    "The extension controlled row is now hidden"
  );

  await extension.unload();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testExtensionControlledHomepageUninstalledAddon() {
  async function checkHomepageEnabled() {
    await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });
    let doc = gBrowser.contentDocument;
    is(
      gBrowser.currentURI.spec,
      "about:preferences#home",
      "#home should be in the URI for about:preferences"
    );

    // The homepage is enabled.
    let homepageInput = doc.getElementById("homePageUrl");
    is(homepageInput.disabled, false, "The homepage input is enabled");
    is(homepageInput.value, "", "The homepage input is empty");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  await ExtensionSettingsStore.initialize();

  // Verify the setting isn't reported as controlled and the inputs are enabled.
  is(
    ExtensionSettingsStore.getSetting("prefs", "homepage_override"),
    null,
    "The homepage_override is not set"
  );
  await checkHomepageEnabled();

  // Disarm any pending writes before we modify the JSONFile directly.
  await ExtensionSettingsStore._reloadFile(false);

  // Write out a bad store file.
  let storeData = {
    prefs: {
      homepage_override: {
        initialValue: "",
        precedenceList: [
          {
            id: "bad@mochi.test",
            installDate: 1508802672,
            value: "https://developer.mozilla.org",
            enabled: true,
          },
        ],
      },
    },
  };
  let jsonFileName = "extension-settings.json";
  let storePath = PathUtils.join(PathUtils.profileDir, jsonFileName);

  await IOUtils.writeUTF8(storePath, JSON.stringify(storeData));

  // Reload the ExtensionSettingsStore so it will read the file on disk. Don't
  // finalize the current store since it will overwrite our file.
  await ExtensionSettingsStore._reloadFile(false);

  // Verify that the setting is reported as set, but the homepage is still enabled
  // since there is no matching installed extension.
  is(
    ExtensionSettingsStore.getSetting("prefs", "homepage_override").value,
    "https://developer.mozilla.org",
    "The homepage_override appears to be set"
  );
  await checkHomepageEnabled();

  // Remove the bad store file that we used.
  await IOUtils.remove(storePath);

  // Reload the ExtensionSettingsStore again so it clears the data we added.
  // Don't finalize the current store since it will write out the bad data.
  await ExtensionSettingsStore._reloadFile(false);

  is(
    ExtensionSettingsStore.getSetting("prefs", "homepage_override"),
    null,
    "The ExtensionSettingsStore is left empty."
  );
});

add_task(async function testExtensionControlledTrackingProtection() {
  const TP_PREF = "privacy.trackingprotection.enabled";
  const TP_DEFAULT = false;
  const EXTENSION_ID = "@set_tp";
  const CONTROLLED_LABEL_ID =
    "contentBlockingTrackingProtectionExtensionContentLabel";
  const CONTROLLED_BUTTON_ID =
    "contentBlockingDisableTrackingProtectionExtension";

  let tpEnabledPref = () => Services.prefs.getBoolPref(TP_PREF);

  await SpecialPowers.pushPrefEnv({ set: [[TP_PREF, TP_DEFAULT]] });

  function background() {
    browser.privacy.websites.trackingProtectionMode.set({ value: "always" });
  }

  function verifyState(isControlled) {
    is(tpEnabledPref(), isControlled, "TP pref is set to the expected value.");

    let controlledLabel = doc.getElementById(CONTROLLED_LABEL_ID);
    let controlledButton = doc.getElementById(CONTROLLED_BUTTON_ID);

    is(
      controlledLabel.hidden,
      !isControlled,
      "The extension controlled row's visibility is as expected."
    );
    is(
      controlledButton.hidden,
      !isControlled,
      "The disable extension button's visibility is as expected."
    );
    if (isControlled) {
      let controlledDesc = controlledLabel.querySelector("description");
      Assert.deepEqual(
        doc.l10n.getAttributes(controlledDesc),
        {
          id: "extension-controlling-websites-content-blocking-all-trackers",
          args: {
            name: "set_tp",
          },
        },
        "The user is notified that an extension is controlling TP."
      );
    }

    is(
      doc.getElementById("trackingProtectionMenu").disabled,
      isControlled,
      "TP control is enabled."
    );
  }

  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;

  is(
    gBrowser.currentURI.spec,
    "about:preferences#privacy",
    "#privacy should be in the URI for about:preferences"
  );

  verifyState(false);

  // Install an extension that sets Tracking Protection.
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      name: "set_tp",
      browser_specific_settings: { gecko: { id: EXTENSION_ID } },
      permissions: ["privacy"],
    },
    background,
  });

  let messageShown = waitForMessageShown(CONTROLLED_LABEL_ID);
  await extension.startup();
  await messageShown;
  let addon = await AddonManager.getAddonByID(EXTENSION_ID);

  verifyState(true);

  await disableExtensionViaClick(
    CONTROLLED_LABEL_ID,
    CONTROLLED_BUTTON_ID,
    doc
  );

  verifyState(false);

  // Enable the extension so we get the UNINSTALL event, which is needed by
  // ExtensionPreferencesManager to clean up properly.
  // TODO: BUG 1408226
  await reEnableExtension(addon, CONTROLLED_LABEL_ID);

  await extension.unload();

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testExtensionControlledPasswordManager() {
  const PASSWORD_MANAGER_ENABLED_PREF = "signon.rememberSignons";
  const PASSWORD_MANAGER_ENABLED_DEFAULT = true;
  const CONTROLLED_BUTTON_ID = "disablePasswordManagerExtension";
  const CONTROLLED_LABEL_ID = "passwordManagerExtensionContent";
  const EXTENSION_ID = "@remember_signons";
  let manifest = {
    manifest_version: 2,
    name: "testPasswordManagerExtension",
    version: "1.0",
    description: "Testing rememberSignons",
    browser_specific_settings: { gecko: { id: EXTENSION_ID } },
    permissions: ["privacy"],
    browser_action: {
      default_title: "Testing rememberSignons",
    },
  };

  let passwordManagerEnabledPref = () =>
    Services.prefs.getBoolPref(PASSWORD_MANAGER_ENABLED_PREF);

  await SpecialPowers.pushPrefEnv({
    set: [[PASSWORD_MANAGER_ENABLED_PREF, PASSWORD_MANAGER_ENABLED_DEFAULT]],
  });
  is(
    passwordManagerEnabledPref(),
    true,
    "Password manager is enabled by default."
  );

  function verifyState(isControlled) {
    is(
      passwordManagerEnabledPref(),
      !isControlled,
      "Password manager pref is set to the expected value."
    );
    let controlledLabel =
      gBrowser.contentDocument.getElementById(CONTROLLED_LABEL_ID);
    let controlledButton =
      gBrowser.contentDocument.getElementById(CONTROLLED_BUTTON_ID);
    is(
      controlledLabel.hidden,
      !isControlled,
      "The extension's controlled row visibility is as expected."
    );
    is(
      controlledButton.hidden,
      !isControlled,
      "The extension's controlled button visibility is as expected."
    );
    if (isControlled) {
      let controlledDesc = controlledLabel.querySelector("description");
      Assert.deepEqual(
        gBrowser.contentDocument.l10n.getAttributes(controlledDesc),
        {
          id: "extension-controlling-password-saving",
          args: {
            name: "testPasswordManagerExtension",
          },
        },
        "The user is notified that an extension is controlling the remember signons pref."
      );
    }
  }

  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });

  info("Verify that no extension is controlling the password manager pref.");
  verifyState(false);

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "permanent",
    background() {
      browser.privacy.services.passwordSavingEnabled.set({ value: false });
    },
  });
  let messageShown = waitForMessageShown(CONTROLLED_LABEL_ID);
  await extension.startup();
  await messageShown;

  info(
    "Verify that the test extension is controlling the password manager pref."
  );
  verifyState(true);

  info("Verify that the extension shows as controlled when loaded again.");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });
  verifyState(true);

  await disableExtensionViaClick(
    CONTROLLED_LABEL_ID,
    CONTROLLED_BUTTON_ID,
    gBrowser.contentDocument
  );

  info(
    "Verify that disabling the test extension removes the lock on the password manager pref."
  );
  verifyState(false);

  await extension.unload();

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testExtensionControlledProxyConfig() {
  const proxySvc = Ci.nsIProtocolProxyService;
  const PROXY_DEFAULT = proxySvc.PROXYCONFIG_SYSTEM;
  const EXTENSION_ID = "@set_proxy";
  const CONTROLLED_SECTION_ID = "proxyExtensionContent";
  const CONTROLLED_BUTTON_ID = "disableProxyExtension";
  const CONNECTION_SETTINGS_DESC_ID = "connectionSettingsDescription";
  const PANEL_URL =
    "chrome://browser/content/preferences/dialogs/connection.xhtml";

  await SpecialPowers.pushPrefEnv({ set: [[PROXY_PREF, PROXY_DEFAULT]] });

  function background() {
    browser.proxy.settings.set({ value: { proxyType: "none" } });
  }

  function expectedConnectionSettingsMessage(doc, isControlled) {
    return isControlled
      ? "extension-controlling-proxy-config"
      : "network-proxy-connection-description";
  }

  function connectionSettingsMessagePromise(doc, isControlled) {
    return waitForMessageContent(
      CONNECTION_SETTINGS_DESC_ID,
      expectedConnectionSettingsMessage(doc, isControlled),
      doc
    );
  }

  function verifyProxyState(doc, isControlled) {
    let isPanel = doc.getElementById(CONTROLLED_BUTTON_ID);
    is(
      proxyType === proxySvc.PROXYCONFIG_DIRECT,
      isControlled,
      "Proxy pref is set to the expected value."
    );

    if (isPanel) {
      let controlledSection = doc.getElementById(CONTROLLED_SECTION_ID);

      is(
        controlledSection.hidden,
        !isControlled,
        "The extension controlled row's visibility is as expected."
      );
      if (isPanel) {
        is(
          doc.getElementById(CONTROLLED_BUTTON_ID).hidden,
          !isControlled,
          "The disable extension button's visibility is as expected."
        );
      }
      if (isControlled) {
        let controlledDesc = controlledSection.querySelector("description");
        Assert.deepEqual(
          doc.l10n.getAttributes(controlledDesc),
          {
            id: "extension-controlling-proxy-config",
            args: {
              name: "set_proxy",
            },
          },
          "The user is notified that an extension is controlling proxy settings."
        );
      }
      function getProxyControls() {
        let controlGroup = doc.getElementById("networkProxyType");
        let manualControlContainer = controlGroup.querySelector("#proxy-grid");
        return {
          manualControls: [
            ...manualControlContainer.querySelectorAll(
              "label[data-l10n-id]:not([control=networkProxyNone])"
            ),
            ...manualControlContainer.querySelectorAll("input"),
            ...manualControlContainer.querySelectorAll("checkbox"),
            ...doc.querySelectorAll("#networkProxySOCKSVersion > radio"),
          ],
          pacControls: [doc.getElementById("networkProxyAutoconfigURL")],
          otherControls: [
            doc.querySelector("label[control=networkProxyNone]"),
            doc.getElementById("networkProxyNone"),
            ...controlGroup.querySelectorAll(":scope > radio"),
            ...doc.querySelectorAll("#ConnectionsDialogPane > checkbox"),
          ],
        };
      }
      let controlState = isControlled ? "disabled" : "enabled";
      let controls = getProxyControls();
      for (let element of controls.manualControls) {
        let disabled =
          isControlled || proxyType !== proxySvc.PROXYCONFIG_MANUAL;
        is(
          element.disabled,
          disabled,
          `Manual proxy controls should be ${controlState} - control: ${element.outerHTML}.`
        );
      }
      for (let element of controls.pacControls) {
        let disabled = isControlled || proxyType !== proxySvc.PROXYCONFIG_PAC;
        is(
          element.disabled,
          disabled,
          `PAC proxy controls should be ${controlState} - control: ${element.outerHTML}.`
        );
      }
      for (let element of controls.otherControls) {
        is(
          element.disabled,
          isControlled,
          `Other proxy controls should be ${controlState} - control: ${element.outerHTML}.`
        );
      }
    } else {
      let elem = doc.getElementById(CONNECTION_SETTINGS_DESC_ID);
      is(
        doc.l10n.getAttributes(elem).id,
        expectedConnectionSettingsMessage(doc, isControlled),
        "The connection settings description is as expected."
      );
    }
  }

  async function reEnableProxyExtension(addon) {
    let messageChanged = connectionSettingsMessagePromise(mainDoc, true);
    await addon.enable();
    await messageChanged;
  }

  async function openProxyPanel() {
    let panel = await openAndLoadSubDialog(PANEL_URL);
    let closingPromise = BrowserTestUtils.waitForEvent(
      panel.document.getElementById("ConnectionsDialog"),
      "dialogclosing"
    );
    ok(panel, "Proxy panel opened.");
    return { panel, closingPromise };
  }

  async function closeProxyPanel(panelObj) {
    let dialog = panelObj.panel.document.getElementById("ConnectionsDialog");
    dialog.cancelDialog();
    let panelClosingEvent = await panelObj.closingPromise;
    ok(panelClosingEvent, "Proxy panel closed.");
  }

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let mainDoc = gBrowser.contentDocument;

  is(
    gBrowser.currentURI.spec,
    "about:preferences#general",
    "#general should be in the URI for about:preferences"
  );

  verifyProxyState(mainDoc, false);

  // Open the connections panel.
  let panelObj = await openProxyPanel();
  let panelDoc = panelObj.panel.document;

  verifyProxyState(panelDoc, false);

  await closeProxyPanel(panelObj);

  verifyProxyState(mainDoc, false);

  // Install an extension that controls proxy settings. The extension needs
  // incognitoOverride because controlling the proxy.settings requires private
  // browsing access.
  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    useAddonManager: "permanent",
    manifest: {
      name: "set_proxy",
      browser_specific_settings: { gecko: { id: EXTENSION_ID } },
      permissions: ["proxy"],
    },
    background,
  });

  let messageChanged = connectionSettingsMessagePromise(mainDoc, true);
  await extension.startup();
  await messageChanged;
  let addon = await AddonManager.getAddonByID(EXTENSION_ID);

  verifyProxyState(mainDoc, true);
  messageChanged = connectionSettingsMessagePromise(mainDoc, false);

  panelObj = await openProxyPanel();
  panelDoc = panelObj.panel.document;

  verifyProxyState(panelDoc, true);

  await disableExtensionViaClick(
    CONTROLLED_SECTION_ID,
    CONTROLLED_BUTTON_ID,
    panelDoc
  );

  verifyProxyState(panelDoc, false);

  await closeProxyPanel(panelObj);
  await messageChanged;

  verifyProxyState(mainDoc, false);

  await reEnableProxyExtension(addon);

  verifyProxyState(mainDoc, true);
  messageChanged = connectionSettingsMessagePromise(mainDoc, false);

  panelObj = await openProxyPanel();
  panelDoc = panelObj.panel.document;

  verifyProxyState(panelDoc, true);

  await disableExtensionViaClick(
    CONTROLLED_SECTION_ID,
    CONTROLLED_BUTTON_ID,
    panelDoc
  );

  verifyProxyState(panelDoc, false);

  await closeProxyPanel(panelObj);
  await messageChanged;

  verifyProxyState(mainDoc, false);

  // Enable the extension so we get the UNINSTALL event, which is needed by
  // ExtensionPreferencesManager to clean up properly.
  // TODO: BUG 1408226
  await reEnableProxyExtension(addon);

  await extension.unload();

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test that the newtab menu selection is correct when loading about:preferences
add_task(async function testMenuSyncFromPrefs() {
  const DEFAULT_NEWTAB = "about:newtab";

  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });
  is(
    gBrowser.currentURI.spec,
    "about:preferences#home",
    "#home should be in the URI for about:preferences"
  );

  let doc = gBrowser.contentDocument;
  let newTabMenuList = doc.getElementById("newTabMode");
  // The new tab page is set to the default.
  is(AboutNewTab.newTabURL, DEFAULT_NEWTAB, "new tab is set to default");

  is(newTabMenuList.value, "0", "New tab menulist is set to the default");

  newTabMenuList.value = "1";
  newTabMenuList.dispatchEvent(new Event("command"));
  is(newTabMenuList.value, "1", "New tab menulist is set to blank");

  gBrowser.reloadTab(gBrowser.selectedTab);

  await TestUtils.waitForCondition(
    () => gBrowser.contentDocument.getElementById("newTabMode"),
    "wait until element exists in new contentDoc"
  );

  is(
    gBrowser.contentDocument.getElementById("newTabMode").value,
    "1",
    "New tab menulist is still set to blank"
  );

  // Cleanup
  newTabMenuList.value = "0";
  newTabMenuList.dispatchEvent(new Event("command"));
  is(AboutNewTab.newTabURL, DEFAULT_NEWTAB, "new tab is set to default");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
