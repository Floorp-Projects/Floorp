/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ADDON_ID = "test-devtools@mozilla.org";
const ADDON_NAME = "test-devtools";

/**
 * Returns a promise that resolves when the given add-on event is fired. The
 * resolved value is an array of arguments passed for the event.
 */
function promiseAddonEvent(event) {
  return new Promise(resolve => {
    let listener = {
      [event]: function (...args) {
        AddonManager.removeAddonListener(listener);
        resolve(args);
      }
    };

    AddonManager.addAddonListener(listener);
  });
}

function* tearDownAddon(addon) {
  const onUninstalled = promiseAddonEvent("onUninstalled");
  addon.uninstall();
  const [uninstalledAddon] = yield onUninstalled;
  is(uninstalledAddon.id, addon.id,
     `Add-on was uninstalled: ${uninstalledAddon.id}`);
}

function getReloadButton(document, addonName) {
  const names = getInstalledAddonNames(document);
  const name = names.filter(element => element.textContent === addonName)[0];
  ok(name, `Found ${addonName} add-on in the list`);
  const targetElement = name.parentNode.parentNode;
  const reloadButton = targetElement.querySelector(".reload-button");
  info(`Found reload button for ${addonName}`);
  return reloadButton;
}

function installAddonWithManager(filePath) {
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

function getAddonByID(addonId) {
  return new Promise(resolve => {
    AddonManager.getAddonByID(addonId, addon => resolve(addon));
  });
}

/**
 * Creates a web extension from scratch in a temporary location.
 * The object must be removed when you're finished working with it.
 */
class TempWebExt {
  constructor(addonId) {
    this.addonId = addonId;
    this.tmpDir = FileUtils.getDir("TmpD", ["browser_addons_reload"]);
    if (!this.tmpDir.exists()) {
      this.tmpDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    }
    this.sourceDir = this.tmpDir.clone();
    this.sourceDir.append(this.addonId);
    if (!this.sourceDir.exists()) {
      this.sourceDir.create(Ci.nsIFile.DIRECTORY_TYPE,
                           FileUtils.PERMS_DIRECTORY);
    }
  }

  writeManifest(manifestData) {
    const manifest = this.sourceDir.clone();
    manifest.append("manifest.json");
    if (manifest.exists()) {
      manifest.remove(true);
    }
    const fos = Cc["@mozilla.org/network/file-output-stream;1"]
                              .createInstance(Ci.nsIFileOutputStream);
    fos.init(manifest,
             FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
             FileUtils.MODE_TRUNCATE,
             FileUtils.PERMS_FILE, 0);

    const manifestString = JSON.stringify(manifestData);
    fos.write(manifestString, manifestString.length);
    fos.close();
  }

  remove() {
    return this.tmpDir.remove(true);
  }
}

add_task(function* reloadButtonReloadsAddon() {
  const { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);
  yield installAddon({
    document,
    path: "addons/unpacked/install.rdf",
    name: ADDON_NAME,
  });

  const reloadButton = getReloadButton(document, ADDON_NAME);
  is(reloadButton.disabled, false, "Reload button should not be disabled");
  is(reloadButton.title, "", "Reload button should not have a tooltip");
  const onInstalled = promiseAddonEvent("onInstalled");

  const onBootstrapInstallCalled = new Promise(done => {
    Services.obs.addObserver(function listener() {
      Services.obs.removeObserver(listener, ADDON_NAME);
      info("Add-on was re-installed: " + ADDON_NAME);
      done();
    }, ADDON_NAME, false);
  });

  reloadButton.click();

  const [reloadedAddon] = yield onInstalled;
  is(reloadedAddon.name, ADDON_NAME,
     "Add-on was reloaded: " + reloadedAddon.name);

  yield onBootstrapInstallCalled;
  yield tearDownAddon(reloadedAddon);
  yield closeAboutDebugging(tab);
});

add_task(function* reloadButtonRefreshesMetadata() {
  const { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);

  const manifestBase = {
    "manifest_version": 2,
    "name": "Temporary web extension",
    "version": "1.0",
    "applications": {
      "gecko": {
        "id": ADDON_ID
      }
    }
  };

  const tempExt = new TempWebExt(ADDON_ID);
  tempExt.writeManifest(manifestBase);

  const onAddonListUpdated = waitForMutation(getTemporaryAddonList(document),
                                             { childList: true });
  const onInstalled = promiseAddonEvent("onInstalled");
  yield AddonManager.installTemporaryAddon(tempExt.sourceDir);
  const [addon] = yield onInstalled;
  info(`addon installed: ${addon.id}`);
  yield onAddonListUpdated;

  const newName = "Temporary web extension (updated)";
  tempExt.writeManifest(Object.assign({}, manifestBase, {name: newName}));

  // Wait for the add-on list to be updated with the reloaded name.
  const onReInstall = promiseAddonEvent("onInstalled");
  const onAddonReloaded = waitForContentMutation(getTemporaryAddonList(document));

  const reloadButton = getReloadButton(document, manifestBase.name);
  reloadButton.click();

  yield onAddonReloaded;
  const [reloadedAddon] = yield onReInstall;
  // Make sure the name was updated correctly.
  const allAddons = getInstalledAddonNames(document)
    .map(element => element.textContent);
  const nameWasUpdated = allAddons.some(name => name === newName);
  ok(nameWasUpdated, `New name appeared in reloaded add-ons: ${allAddons}`);

  yield tearDownAddon(reloadedAddon);
  tempExt.remove();
  yield closeAboutDebugging(tab);
});

add_task(function* onlyTempInstalledAddonsCanBeReloaded() {
  const { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);
  const onAddonListUpdated = waitForMutation(getAddonList(document),
                                             { childList: true });
  yield installAddonWithManager(getSupportsFile("addons/bug1273184.xpi").file);
  yield onAddonListUpdated;
  const addon = yield getAddonByID("bug1273184@tests");

  const reloadButton = getReloadButton(document, addon.name);
  ok(reloadButton, "Reload button exists");
  is(reloadButton.disabled, true, "Reload button should be disabled");
  ok(reloadButton.title, "Disabled reload button should have a tooltip");

  yield tearDownAddon(addon);
  yield closeAboutDebugging(tab);
});
