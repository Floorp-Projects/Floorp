XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const TEST_DIR = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
const CHROME_URL_ROOT = TEST_DIR + "/";

function getSupportsFile(path) {
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Ci.nsIChromeRegistry);
  let uri = Services.io.newURI(CHROME_URL_ROOT + path);
  let fileurl = cr.convertChromeURL(uri);
  return fileurl.QueryInterface(Ci.nsIFileURL);
}

function installAddon(xpiName) {
  let filePath = getSupportsFile(`addons/${xpiName}`).file;
  return new Promise((resolve, reject) => {
    AddonManager.getInstallForFile(filePath, install => {
      if (!install) {
        throw new Error(`An install was not created for ${filePath}`);
      }
      install.addListener({
        onDownloadFailed: reject,
        onDownloadCancelled: reject,
        onInstallFailed: reject,
        onInstallCancelled: reject,
        onInstallEnded: resolve
      });
      install.install();
    });
  });
}

function waitForMutation(target, opts, cb) {
  return new Promise((resolve) => {
    let observer = new MutationObserver(() => {
      if (!cb || cb(target)) {
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(target, opts);
  });
}

function waitForMessageChange(messageId, cb) {
  return waitForMutation(
    gBrowser.contentDocument.getElementById(messageId),
    { attributes: true, attributeFilter: ["hidden"] }, cb);
}

function waitForMessageHidden(messageId) {
  return waitForMessageChange(messageId, target => target.hidden);
}

function waitForMessageShown(messageId) {
  return waitForMessageChange(messageId, target => !target.hidden);
}

add_task(async function testExtensionControlledHomepage() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let doc = gBrowser.contentDocument;
  is(gBrowser.currentURI.spec, "about:preferences#general",
     "#general should be in the URI for about:preferences");
  let homepagePref = () => Services.prefs.getCharPref("browser.startup.homepage");
  let originalHomepagePref = homepagePref();
  let extensionHomepage = "https://developer.mozilla.org/";
  let controlledContent = doc.getElementById("browserHomePageExtensionContent");

  // The homepage is set to the default and editable.
  ok(originalHomepagePref != extensionHomepage, "homepage is empty by default");
  is(doc.getElementById("browserHomePage").disabled, false, "The homepage input is enabled");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  // Install an extension that will set the homepage.
  await installAddon("set_homepage.xpi");
  await waitForMessageShown("browserHomePageExtensionContent");

  // The homepage has been set by the extension, the user is notified and it isn't editable.
  let controlledLabel = controlledContent.querySelector("description");
  is(homepagePref(), extensionHomepage, "homepage is set by extension");
  // There are two spaces before "set_homepage" because it's " <image /> set_homepage".
  is(controlledLabel.textContent, "An extension,  set_homepage, controls your home page.",
     "The user is notified that an extension is controlling the homepage");
  is(controlledContent.hidden, false, "The extension controlled row is hidden");
  is(doc.getElementById("browserHomePage").disabled, true, "The homepage input is disabled");

  // Disable the extension.
  doc.getElementById("disableHomePageExtension").click();

  await waitForMessageHidden("browserHomePageExtensionContent");

  is(homepagePref(), originalHomepagePref, "homepage is set back to default");
  is(doc.getElementById("browserHomePage").disabled, false, "The homepage input is enabled");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  let addon = await AddonManager.getAddonByID("@set_homepage");
  // Enable the extension so we get the UNINSTALL event, which is needed by
  // ExtensionPreferencesManager to clean up properly.
  // FIXME: See https://bugzilla.mozilla.org/show_bug.cgi?id=1408226.
  addon.userDisabled = false;
  await waitForMessageShown("browserHomePageExtensionContent");
  // Do the uninstall now that the enable code has been run.
  addon.uninstall();

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testPrefLockedHomepage() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let doc = gBrowser.contentDocument;
  is(gBrowser.currentURI.spec, "about:preferences#general",
     "#general should be in the URI for about:preferences");

  let homePagePref = "browser.startup.homepage";
  let buttonPrefs = [
    "pref.browser.homepage.disable_button.current_page",
    "pref.browser.homepage.disable_button.bookmark_page",
    "pref.browser.homepage.disable_button.restore_default",
  ];
  let homePageInput = doc.getElementById("browserHomePage");
  let prefs = Services.prefs.getDefaultBranch(null);
  let mutationOpts = {attributes: true, attributeFilter: ["disabled"]};
  let controlledContent = doc.getElementById("browserHomePageExtensionContent");

  // Helper functions.
  let getButton = pref => doc.querySelector(`.homepage-button[preference="${pref}"`);
  let waitForAllMutations = () => Promise.all(
    buttonPrefs.map(pref => waitForMutation(getButton(pref), mutationOpts))
    .concat([waitForMutation(homePageInput, mutationOpts)]));
  let getHomepage = () => Services.prefs.getCharPref("browser.startup.homepage");

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

  ok(originalHomepage != extensionHomepage, "The extension will change the homepage");

  // Install an extension that sets the homepage to MDN.
  await installAddon("set_homepage.xpi");
  await waitForMessageShown(controlledContent.id);

  // Check that everything is still disabled, homepage didn't change.
  is(getHomepage(), extensionHomepage, "The reported homepage is set by the extension");
  is(homePageInput.value, extensionHomepage, "The homepage is set by the extension");
  is(homePageInput.disabled, true, "Homepage is disabled when set by extension");
  buttonPrefs.forEach(pref => {
    is(getButton(pref).disabled, true, `${pref} is disabled when set by extension`);
  });
  is(controlledContent.hidden, false, "The extension controlled message is shown");

  // Lock all of the prefs, wait for the UI to update.
  let messageHidden = waitForMessageHidden(controlledContent.id);
  lockPrefs();
  await messageHidden;

  // Check that everything is now disabled.
  is(getHomepage(), lockedHomepage, "The reported homepage is set by the pref");
  is(homePageInput.value, lockedHomepage, "The homepage is set by the pref");
  is(homePageInput.disabled, true, "The homepage is disabed when the pref is locked");
  buttonPrefs.forEach(pref => {
    is(getButton(pref).disabled, true, `The ${pref} button is disabled when locked`);
  });
  is(controlledContent.hidden, true, "The extension controlled message is hidden when locked");

  // Unlock the prefs, wait for the UI to update.
  let messageShown = waitForMessageShown(controlledContent.id);
  unlockPrefs();
  await messageShown;

  // Verify that the UI is showing the extension's settings.
  is(homePageInput.value, extensionHomepage, "The homepage is set by the extension");
  is(homePageInput.disabled, true, "Homepage is disabled when set by extension");
  buttonPrefs.forEach(pref => {
    is(getButton(pref).disabled, true, `${pref} is disabled when set by extension`);
  });
  is(controlledContent.hidden, false, "The extension controlled message is shown when unlocked");

  // Uninstall the add-on.
  let addon = await AddonManager.getAddonByID("@set_homepage");
  addon.uninstall();
  await waitForMessageHidden(controlledContent.id);

  // Check that everything is now enabled again.
  is(getHomepage(), originalHomepage, "The reported homepage is reset to original value");
  is(homePageInput.value, "", "The homepage is empty");
  is(homePageInput.disabled, false, "The homepage is enabled after clearing lock");
  buttonPrefs.forEach(pref => {
    is(getButton(pref).disabled, false, `The ${pref} button is enabled when unlocked`);
  });

  // Lock the prefs without an extension.
  lockPrefs();
  await waitForAllMutations();

  // Check that everything is now disabled.
  is(getHomepage(), lockedHomepage, "The reported homepage is set by the pref");
  is(homePageInput.value, lockedHomepage, "The homepage is set by the pref");
  is(homePageInput.disabled, true, "The homepage is disabed when the pref is locked");
  buttonPrefs.forEach(pref => {
    is(getButton(pref).disabled, true, `The ${pref} button is disabled when locked`);
  });

  // Unlock the prefs without an extension.
  unlockPrefs();
  await waitForAllMutations();

  // Check that everything is enabled again.
  is(getHomepage(), originalHomepage, "The homepage is reset to the original value");
  is(homePageInput.value, "", "The homepage is clear after being unlocked");
  is(homePageInput.disabled, false, "The homepage is enabled after clearing lock");
  buttonPrefs.forEach(pref => {
    is(getButton(pref).disabled, false, `The ${pref} button is enabled when unlocked`);
  });
  is(controlledContent.hidden, true,
     "The extension controlled message is hidden when unlocked with no extension");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testExtensionControlledNewTab() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let doc = gBrowser.contentDocument;
  is(gBrowser.currentURI.spec, "about:preferences#general",
     "#general should be in the URI for about:preferences");

  let controlledContent = doc.getElementById("browserNewTabExtensionContent");

  // The new tab is set to the default and message is hidden.
  ok(!aboutNewTabService.newTabURL.startsWith("moz-extension:"), "new tab is not set");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  // Install an extension that will set the new tab page.
  await installAddon("set_newtab.xpi");

  await waitForMessageShown("browserNewTabExtensionContent");

  // The new tab page has been set by the extension and the user is notified.
  let controlledLabel = controlledContent.querySelector("description");
  ok(aboutNewTabService.newTabURL.startsWith("moz-extension:"), "new tab url is set by extension");
  // There are two spaces before "set_newtab" because it's " <image /> set_newtab".
  is(controlledLabel.textContent, "An extension,  set_newtab, controls your New Tab page.",
     "The user is notified that an extension is controlling the new tab page");
  is(controlledContent.hidden, false, "The extension controlled row is hidden");

  // Disable the extension.
  doc.getElementById("disableNewTabExtension").click();

  await waitForMessageHidden("browserNewTabExtensionContent");

  ok(!aboutNewTabService.newTabURL.startsWith("moz-extension:"), "new tab page is set back to default");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testExtensionControlledHomepageUninstalledAddon() {
  async function checkHomepageEnabled() {
    await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
    let doc = gBrowser.contentDocument;
    is(gBrowser.currentURI.spec, "about:preferences#general",
      "#general should be in the URI for about:preferences");
    let controlledContent = doc.getElementById("browserHomePageExtensionContent");

    // The homepage is enabled.
    let homepageInut = doc.getElementById("browserHomePage");
    is(homepageInut.disabled, false, "The homepage input is enabled");
    is(homepageInut.value, "", "The homepage input is empty");
    is(controlledContent.hidden, true, "The extension controlled row is hidden");

    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  await ExtensionSettingsStore.initialize();

  // Verify the setting isn't reported as controlled and the inputs are enabled.
  is(ExtensionSettingsStore.getSetting("prefs", "homepage_override"), null,
     "The homepage_override is not set");
  await checkHomepageEnabled();

  // Write out a bad store file.
  let storeData = {
    prefs: {
      homepage_override: {
        initialValue: "",
        precedenceList: [{
          id: "bad@mochi.test",
          installDate: 1508802672,
          value: "https://developer.mozilla.org",
          enabled: true,
        }],
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
  is(ExtensionSettingsStore.getSetting("prefs", "homepage_override").value,
      "https://developer.mozilla.org",
      "The homepage_override appears to be set");
  await checkHomepageEnabled();

  // Remove the bad store file that we used.
  await OS.File.remove(storePath)

  // Reload the ExtensionSettingsStore again so it clears the data we added.
  // Don't finalize the current store since it will write out the bad data.
  await ExtensionSettingsStore._reloadFile(false);

  is(ExtensionSettingsStore.getSetting("prefs", "homepage_override"), null,
     "The ExtensionSettingsStore is left empty.");
});
