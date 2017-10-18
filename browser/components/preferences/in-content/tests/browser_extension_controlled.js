XPCOMUtils.defineLazyModuleGetter(
  this, "AddonTestUtils", "resource://testing-common/AddonTestUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});
const {
  createAppInfo,
  promiseCompleteInstall,
  promiseFindAddonUpdates,
  UpdateServer,
} = AddonTestUtils;

AddonTestUtils.initMochitest(this);

// Allow for unsigned addons.
AddonTestUtils.overrideCertDB();

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

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
  is(controlledContent.hidden, true, "The extension controlled row is shown");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testExtensionControlledDefaultSearch() {
  await openPreferencesViaOpenPreferencesAPI("paneSearch", {leaveOpen: true});
  let doc = gBrowser.contentDocument;
  let extensionId = "@set_default_search";
  let testServer = new UpdateServer();
  let manifest = {
    manifest_version: 2,
    name: "set_default_search",
    applications: { gecko: { id: extensionId, update_url: testServer.updateUrl } },
    description: "set_default_search description",
    permissions: [],
    chrome_settings_overrides: {
      search_provider: {
        name: "Yahoo",
        search_url: "https://search.yahoo.com/yhs/search?p=%s&ei=UTF-8&hspart=mozilla&hsimp=yhs-002",
        is_default: true,
      },
    }
  };
  testServer.serveUpdate({
    manifest: Object.assign({}, manifest, {version: "2.0"}),
  });

  function setEngine(engine) {
    doc.querySelector(`#defaultEngine menuitem[label="${engine.name}"]`)
       .doCommand();
  }

  is(gBrowser.currentURI.spec, "about:preferences#search",
     "#search should be in the URI for about:preferences");

  let controlledContent = doc.getElementById("browserDefaultSearchExtensionContent");
  let initialEngine = Services.search.currentEngine;

  // Ensure the controlled content is hidden when not controlled.
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  // Install an extension that will set the default search engine.
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: Object.assign({}, manifest, {version: "1.0"}),
  });

  let messageShown = waitForMessageShown("browserDefaultSearchExtensionContent");
  await extension.startup();
  await messageShown;

  let addon = await AddonManager.getAddonByID(extensionId);
  is(addon.version, "1.0", "The addon has the expected version.");

  // The default search engine has been set by the extension and the user is notified.
  let controlledLabel = controlledContent.querySelector("description");
  let extensionEngine = Services.search.currentEngine;
  ok(initialEngine != extensionEngine, "The default engine has changed.");
  // There are two spaces before "set_default_search" because it's " <image /> set_default_search".
  is(controlledLabel.textContent,
     "An extension,  set_default_search, has set your default search engine.",
     "The user is notified that an extension is controlling the default search engine");
  is(controlledContent.hidden, false, "The extension controlled row is shown");

  // Set the engine back to the initial one, ensure the message is hidden.
  setEngine(initialEngine);
  await waitForMessageHidden(controlledContent.id);

  is(initialEngine, Services.search.currentEngine,
     "default search engine is set back to default");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  // Setting the engine back to the extension's engine does not show the message.
  setEngine(extensionEngine);

  is(extensionEngine, Services.search.currentEngine,
     "default search engine is set back to extension");
  is(controlledContent.hidden, true, "The extension controlled row is still hidden");

  // Set the engine to the initial one and verify an upgrade doesn't change it.
  setEngine(initialEngine);
  await waitForMessageHidden(controlledContent.id);

  // Update the extension and wait for "ready".
  let update = await promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;
  let awaitStartup = new Promise(resolve => {
    Management.on("ready", function listener(type, readyAddon) {
      if (readyAddon.id == addon.id) {
        resolve(readyAddon);
        Management.off("ready", listener);
      }
    });
  });
  await promiseCompleteInstall(install);
  addon = await awaitStartup;

  // Verify the extension is updated and search engine didn't change.
  is(addon.version, "2.0", "The updated addon has the expected version");
  is(controlledContent.hidden, true, "The extension controlled row is hidden after update");
  is(initialEngine, Services.search.currentEngine,
     "default search engine is still the initial engine after update");

  await testServer.cleanup();
  await extension.unload();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
