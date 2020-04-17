/* eslint-env webextensions */

const PROXY_PREF = "network.proxy.type";

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionSettingsStore",
  "resource://gre/modules/ExtensionSettingsStore.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AboutNewTab",
  "resource:///modules/AboutNewTab.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(this, "proxyType", PROXY_PREF);

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
AddonTestUtils.initMochitest(this);

const TEST_DIR = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
const CHROME_URL_ROOT = TEST_DIR + "/";
const PERMISSIONS_URL =
  "chrome://browser/content/preferences/sitePermissions.xhtml";
let sitePermissionsDialog;

function getSupportsFile(path) {
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
    Ci.nsIChromeRegistry
  );
  let uri = Services.io.newURI(CHROME_URL_ROOT + path);
  let fileurl = cr.convertChromeURL(uri);
  return fileurl.QueryInterface(Ci.nsIFileURL);
}

async function installAddon(xpiName) {
  let filePath = getSupportsFile(`addons/${xpiName}`).file;
  let install = await AddonManager.getInstallForFile(filePath);
  if (!install) {
    throw new Error(`An install was not created for ${filePath}`);
  }
  return new Promise((resolve, reject) => {
    install.addListener({
      onDownloadFailed: reject,
      onDownloadCancelled: reject,
      onInstallFailed: reject,
      onInstallCancelled: reject,
      onInstallEnded: resolve,
    });
    install.install();
  });
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

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    let doc = content.document;
    let settingsButton = doc.getElementById("notificationSettingsButton");
    settingsButton.click();
  });

  sitePermissionsDialog = await dialogOpened;
  await sitePermissionsDialog.document.mozSubdialogReady;
}

add_task(async function testExtensionControlledHomepage() {
  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });
  let doc = gBrowser.contentDocument;
  is(
    gBrowser.currentURI.spec,
    "about:preferences#home",
    "#home should be in the URI for about:preferences"
  );
  let homepagePref = () =>
    Services.prefs.getCharPref("browser.startup.homepage");
  let originalHomepagePref = homepagePref();
  let extensionHomepage = "https://developer.mozilla.org/";
  let controlledContent = doc.getElementById("browserHomePageExtensionContent");

  let homeModeEl = doc.getElementById("homeMode");
  let customSettingsSection = doc.getElementById("customSettings");

  // The homepage is set to the default and the custom settings section is hidden
  ok(originalHomepagePref != extensionHomepage, "homepage is empty by default");
  is(homeModeEl.disabled, false, "The homepage menulist is enabled");
  is(
    customSettingsSection.hidden,
    true,
    "The custom settings element is hidden"
  );
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  // Install an extension that will set the homepage.
  let promise = waitForMessageShown("browserHomePageExtensionContent");
  await installAddon("set_homepage.xpi");
  await promise;

  // The homepage has been set by the extension, the user is notified and it isn't editable.
  let controlledLabel = controlledContent.querySelector("description");
  is(homepagePref(), extensionHomepage, "homepage is set by extension");
  Assert.deepEqual(
    doc.l10n.getAttributes(controlledLabel),
    {
      id: "extension-controlled-homepage-override",
      args: {
        name: "set_homepage",
      },
    },
    "The user is notified that an extension is controlling the homepage"
  );
  is(controlledContent.hidden, false, "The extension controlled row is hidden");
  is(homeModeEl.disabled, true, "The homepage input is disabled");

  // Disable the extension.
  let enableMessageShown = waitForEnableMessage(controlledContent.id);
  doc.getElementById("disableHomePageExtension").click();
  await enableMessageShown;

  // The user is notified how to enable the extension.
  is(
    doc.l10n.getAttributes(controlledLabel.querySelector("label")).id,
    "extension-controlled-enable",
    "The user is notified of how to enable the extension again"
  );

  // The user can dismiss the enable instructions.
  let hidden = waitForMessageHidden("browserHomePageExtensionContent");
  controlledLabel.querySelector("image:last-of-type").click();
  await hidden;

  // The homepage elements are reset to their original state.
  is(homepagePref(), originalHomepagePref, "homepage is set back to default");
  is(homeModeEl.disabled, false, "The homepage menulist is enabled");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  // Cleanup the add-on and tab.
  let addon = await AddonManager.getAddonByID("@set_homepage");
  // Enable the extension so we get the UNINSTALL event, which is needed by
  // ExtensionPreferencesManager to clean up properly.
  // FIXME: See https://bugzilla.mozilla.org/show_bug.cgi?id=1408226.
  promise = waitForMessageShown("browserHomePageExtensionContent");
  await addon.enable();
  await promise;
  // Do the uninstall now that the enable code has been run.
  await addon.uninstall();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testPrefLockedHomepage() {
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
  let controlledContent = doc.getElementById("browserHomePageExtensionContent");

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

  ok(
    originalHomepage != extensionHomepage,
    "The extension will change the homepage"
  );

  // Install an extension that sets the homepage to MDN.
  let promise = waitForMessageShown(controlledContent.id);
  await installAddon("set_homepage.xpi");
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
  is(
    homePageInput.disabled,
    true,
    "Homepage custom input is disabled when set by extension"
  );
  is(
    homeModeEl.disabled,
    true,
    "Homepage menulist is disabled when set by extension"
  );
  buttonPrefs.forEach(pref => {
    is(
      getButton(pref).disabled,
      true,
      `${pref} is disabled when set by extension`
    );
  });
  is(
    controlledContent.hidden,
    false,
    "The extension controlled message is shown"
  );

  // Lock all of the prefs, wait for the UI to update.
  let messageHidden = waitForMessageHidden(controlledContent.id);

  lockPrefs();
  await messageHidden;

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
    "Homepage menulist is disabled when the pref is locked"
  );

  buttonPrefs.forEach(pref => {
    is(
      getButton(pref).disabled,
      true,
      `The ${pref} button is disabled when locked`
    );
  });
  is(
    controlledContent.hidden,
    true,
    "The extension controlled message is hidden when locked"
  );

  // Unlock the prefs, wait for the UI to update.
  let messageShown = waitForMessageShown(controlledContent.id);
  unlockPrefs();
  await messageShown;

  // Verify that the UI is showing the extension's settings.
  is(
    homePageInput.value,
    extensionHomepage,
    "The homepage is set by the extension"
  );
  is(
    homePageInput.disabled,
    true,
    "Homepage is disabled when set by extension"
  );
  is(
    homeModeEl.disabled,
    true,
    "Homepage menulist is disabled when set by extension"
  );
  buttonPrefs.forEach(pref => {
    is(
      getButton(pref).disabled,
      true,
      `${pref} is disabled when set by extension`
    );
  });
  is(
    controlledContent.hidden,
    false,
    "The extension controlled message is shown when unlocked"
  );

  // Uninstall the add-on.
  let addon = await AddonManager.getAddonByID("@set_homepage");
  promise = waitForEnableMessage(controlledContent.id);
  await addon.uninstall();
  await promise;

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
  let mutationsDone = waitForAllMutations();
  lockPrefs();
  await mutationsDone;

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
    "Homepage menulist is disabled when prefis locked"
  );
  buttonPrefs.forEach(pref => {
    is(
      getButton(pref).disabled,
      true,
      `The ${pref} button is disabled when locked`
    );
  });

  // Unlock the prefs without an extension.
  unlockPrefs();
  await waitForAllMutations();

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
  is(
    controlledContent.hidden,
    true,
    "The extension controlled message is hidden when unlocked with no extension"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testExtensionControlledNewTab() {
  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });
  let doc = gBrowser.contentDocument;
  is(
    gBrowser.currentURI.spec,
    "about:preferences#home",
    "#home should be in the URI for about:preferences"
  );

  let controlledContent = doc.getElementById("browserNewTabExtensionContent");

  // The new tab is set to the default and message is hidden.
  ok(!AboutNewTab.newTabURL.startsWith("moz-extension:"), "new tab is not set");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  // Install an extension that will set the new tab page.
  let promise = waitForMessageShown("browserNewTabExtensionContent");
  await installAddon("set_newtab.xpi");
  await promise;

  // The new tab page has been set by the extension and the user is notified.
  let controlledLabel = controlledContent.querySelector("description");
  ok(
    AboutNewTab.newTabURL.startsWith("moz-extension:"),
    "new tab url is set by extension"
  );
  Assert.deepEqual(
    doc.l10n.getAttributes(controlledLabel),
    {
      id: "extension-controlled-new-tab-url",
      args: {
        name: "set_newtab",
      },
    },
    "The user is notified that an extension is controlling the new tab page"
  );
  is(controlledContent.hidden, false, "The extension controlled row is hidden");

  // Disable the extension.
  doc.getElementById("disableNewTabExtension").click();

  // Verify the user is notified how to enable the extension.
  await waitForEnableMessage(controlledContent.id);
  is(
    doc.l10n.getAttributes(controlledLabel.querySelector("label")).id,
    "extension-controlled-enable",
    "The user is notified of how to enable the extension again"
  );

  // Verify the enable message can be dismissed.
  let hidden = waitForMessageHidden(controlledContent.id);
  let dismissButton = controlledLabel.querySelector("image:last-of-type");
  dismissButton.click();
  await hidden;

  // Ensure the New Tab page has been reset and there is no message.
  ok(
    !AboutNewTab.newTabURL.startsWith("moz-extension:"),
    "new tab page is set back to default"
  );
  is(controlledContent.hidden, true, "The extension controlled row is shown");

  // Cleanup the tab and add-on.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  let addon = await AddonManager.getAddonByID("@set_newtab");
  await addon.uninstall();
});

add_task(async function testExtensionControlledWebNotificationsPermission() {
  let manifest = {
    manifest_version: 2,
    name: "TestExtension",
    version: "1.0",
    description: "Testing WebNotificationsDisable",
    applications: { gecko: { id: "@web_notifications_disable" } },
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
      id: "extension-controlled-web-notifications",
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

add_task(async function testExtensionControlledDefaultSearch() {
  await openPreferencesViaOpenPreferencesAPI("paneSearch", { leaveOpen: true });
  let doc = gBrowser.contentDocument;
  let extensionId = "@set_default_search";
  let manifest = {
    manifest_version: 2,
    name: "set_default_search",
    applications: { gecko: { id: extensionId } },
    description: "set_default_search description",
    permissions: [],
    chrome_settings_overrides: {
      search_provider: {
        name: "DuckDuckGo",
        search_url: "https://duckduckgo.com/?q={searchTerms}",
        is_default: true,
      },
    },
  };

  // This test is comparing nsISearchEngines by reference, so we need to initialize
  // the SearchService here.
  await Services.search.init();

  function setEngine(engine) {
    doc
      .querySelector(`#defaultEngine menuitem[label="${engine.name}"]`)
      .doCommand();
  }

  is(
    gBrowser.currentURI.spec,
    "about:preferences#search",
    "#search should be in the URI for about:preferences"
  );

  let controlledContent = doc.getElementById(
    "browserDefaultSearchExtensionContent"
  );
  let initialEngine = Services.search.defaultEngine;

  // Ensure the controlled content is hidden when not controlled.
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  // Install an extension that will set the default search engine.
  let originalExtension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: Object.assign({}, manifest, { version: "1.0" }),
  });

  let messageShown = waitForMessageShown(
    "browserDefaultSearchExtensionContent"
  );
  await originalExtension.startup();
  await AddonTestUtils.waitForSearchProviderStartup(originalExtension);
  await messageShown;

  let addon = await AddonManager.getAddonByID(extensionId);
  is(addon.version, "1.0", "The addon has the expected version.");

  // The default search engine has been set by the extension and the user is notified.
  let controlledLabel = controlledContent.querySelector("description");
  let extensionEngine = Services.search.defaultEngine;
  ok(initialEngine != extensionEngine, "The default engine has changed.");
  Assert.deepEqual(
    doc.l10n.getAttributes(controlledLabel),
    {
      id: "extension-controlled-default-search",
      args: {
        name: "set_default_search",
      },
    },
    "The user is notified that an extension is controlling the default search engine"
  );
  is(controlledContent.hidden, false, "The extension controlled row is shown");

  // Set the engine back to the initial one, ensure the message is hidden.
  setEngine(initialEngine);
  await waitForMessageHidden(controlledContent.id);

  is(
    initialEngine,
    Services.search.defaultEngine,
    "default search engine is set back to default"
  );
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  // Setting the engine back to the extension's engine does not show the message.
  setEngine(extensionEngine);
  // Wait a tick for the Search Service's promises to resolve.
  await new Promise(resolve => executeSoon(resolve));

  is(
    extensionEngine,
    Services.search.defaultEngine,
    "default search engine is set back to extension"
  );
  is(
    controlledContent.hidden,
    true,
    "The extension controlled row is still hidden"
  );

  // Set the engine to the initial one and verify an upgrade doesn't change it.
  setEngine(initialEngine);
  await waitForMessageHidden(controlledContent.id);

  // Update the extension and wait for "ready".
  let updatedExtension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: Object.assign({}, manifest, { version: "2.0" }),
  });
  await updatedExtension.startup();
  await AddonTestUtils.waitForSearchProviderStartup(updatedExtension);
  addon = await AddonManager.getAddonByID(extensionId);

  // Verify the extension is updated and search engine didn't change.
  is(addon.version, "2.0", "The updated addon has the expected version");
  is(
    controlledContent.hidden,
    true,
    "The extension controlled row is hidden after update"
  );
  is(
    initialEngine,
    Services.search.defaultEngine,
    "default search engine is still the initial engine after update"
  );

  await originalExtension.unload();
  await updatedExtension.unload();
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
    let controlledContent = doc.getElementById(
      "browserHomePageExtensionContent"
    );

    // The homepage is enabled.
    let homepageInput = doc.getElementById("homePageUrl");
    is(homepageInput.disabled, false, "The homepage input is enabled");
    is(homepageInput.value, "", "The homepage input is empty");
    is(
      controlledContent.hidden,
      true,
      "The extension controlled row is hidden"
    );

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
  let storePath = OS.Path.join(OS.Constants.Path.profileDir, jsonFileName);
  await OS.File.writeAtomic(storePath, JSON.stringify(storeData));

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
  await OS.File.remove(storePath);

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
  const DISABLE_BUTTON_ID = "contentBlockingDisableTrackingProtectionExtension";

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
          id: "extension-controlled-websites-content-blocking-all-trackers",
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

  async function disableViaClick() {
    let labelId = CONTROLLED_LABEL_ID;
    let disableId = DISABLE_BUTTON_ID;
    let controlledLabel = doc.getElementById(labelId);

    let enableMessageShown = waitForEnableMessage(labelId);
    doc.getElementById(disableId).click();
    await enableMessageShown;

    // The user is notified how to enable the extension.
    let controlledDesc = controlledLabel.querySelector("description");
    is(
      doc.l10n.getAttributes(controlledDesc.querySelector("label")).id,
      "extension-controlled-enable",
      "The user is notified of how to enable the extension again"
    );

    // The user can dismiss the enable instructions.
    let hidden = waitForMessageHidden(labelId);
    controlledLabel.querySelector("image:last-of-type").click();
    await hidden;
  }

  async function reEnableExtension(addon) {
    let controlledMessageShown = waitForMessageShown(CONTROLLED_LABEL_ID);
    await addon.enable();
    await controlledMessageShown;
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
      applications: { gecko: { id: EXTENSION_ID } },
      permissions: ["privacy"],
    },
    background,
  });

  let messageShown = waitForMessageShown(CONTROLLED_LABEL_ID);
  await extension.startup();
  await messageShown;
  let addon = await AddonManager.getAddonByID(EXTENSION_ID);

  verifyState(true);

  await disableViaClick();

  verifyState(false);

  // Enable the extension so we get the UNINSTALL event, which is needed by
  // ExtensionPreferencesManager to clean up properly.
  // TODO: BUG 1408226
  await reEnableExtension(addon);

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
  const PANEL_URL = "chrome://browser/content/preferences/connection.xhtml";

  await SpecialPowers.pushPrefEnv({ set: [[PROXY_PREF, PROXY_DEFAULT]] });

  function background() {
    browser.proxy.settings.set({ value: { proxyType: "none" } });
  }

  function expectedConnectionSettingsMessage(doc, isControlled) {
    return isControlled
      ? "extension-controlled-proxy-config"
      : "network-proxy-connection-description";
  }

  function connectionSettingsMessagePromise(doc, isControlled) {
    return waitForMessageContent(
      CONNECTION_SETTINGS_DESC_ID,
      expectedConnectionSettingsMessage(doc, isControlled),
      doc
    );
  }

  function verifyState(doc, isControlled) {
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
            id: "extension-controlled-proxy-config",
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

  async function disableViaClick() {
    let sectionId = CONTROLLED_SECTION_ID;
    let controlledSection = panelDoc.getElementById(sectionId);

    let enableMessageShown = waitForEnableMessage(sectionId, panelDoc);
    panelDoc.getElementById(CONTROLLED_BUTTON_ID).click();
    await enableMessageShown;

    // The user is notified how to enable the extension.
    let controlledDesc = controlledSection.querySelector("description");
    is(
      panelDoc.l10n.getAttributes(controlledDesc.querySelector("label")).id,
      "extension-controlled-enable",
      "The user is notified of how to enable the extension again"
    );

    // The user can dismiss the enable instructions.
    let hidden = waitForMessageHidden(sectionId, panelDoc);
    controlledSection.querySelector("image:last-of-type").click();
    return hidden;
  }

  async function reEnableExtension(addon) {
    let messageChanged = connectionSettingsMessagePromise(mainDoc, true);
    await addon.enable();
    await messageChanged;
  }

  async function openProxyPanel() {
    let panel = await openAndLoadSubDialog(PANEL_URL);
    let closingPromise = waitForEvent(
      panel.document.getElementById("ConnectionsDialog"),
      "dialogclosing"
    );
    ok(panel, "Proxy panel opened.");
    return { panel, closingPromise };
  }

  async function closeProxyPanel(panelObj) {
    let dialog = panelObj.panel.document.getElementById("ConnectionsDialog");
    dialog.cancelDialog();
    await panelObj.closingPromise;
    is(panelObj.pane.state, "closed", "Proxy panel closed.");
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

  verifyState(mainDoc, false);

  // Open the connections panel.
  let panelObj = await openProxyPanel();
  let panelDoc = panelObj.panel.document;

  verifyState(panelDoc, false);

  await closeProxyPanel(panelObj);

  verifyState(mainDoc, false);

  // Install an extension that controls proxy settings. The extension needs
  // incognitoOverride because controlling the proxy.settings requires private
  // browsing access.
  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    useAddonManager: "permanent",
    manifest: {
      name: "set_proxy",
      applications: { gecko: { id: EXTENSION_ID } },
      permissions: ["proxy"],
    },
    background,
  });

  let messageChanged = connectionSettingsMessagePromise(mainDoc, true);
  await extension.startup();
  await messageChanged;
  let addon = await AddonManager.getAddonByID(EXTENSION_ID);

  verifyState(mainDoc, true);
  messageChanged = connectionSettingsMessagePromise(mainDoc, false);

  panelObj = await openProxyPanel();
  panelDoc = panelObj.panel.document;

  verifyState(panelDoc, true);

  await disableViaClick();

  verifyState(panelDoc, false);

  await closeProxyPanel(panelObj);
  await messageChanged;

  verifyState(mainDoc, false);

  await reEnableExtension(addon);

  verifyState(mainDoc, true);
  messageChanged = connectionSettingsMessagePromise(mainDoc, false);

  panelObj = await openProxyPanel();
  panelDoc = panelObj.panel.document;

  verifyState(panelDoc, true);

  await disableViaClick();

  verifyState(panelDoc, false);

  await closeProxyPanel(panelObj);
  await messageChanged;

  verifyState(mainDoc, false);

  // Enable the extension so we get the UNINSTALL event, which is needed by
  // ExtensionPreferencesManager to clean up properly.
  // TODO: BUG 1408226
  await reEnableExtension(addon);

  await extension.unload();

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
