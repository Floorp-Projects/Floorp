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

function getReloadButton(document, addonName) {
  const names = [...document.querySelectorAll("#addons .target-name")];
  const name = names.filter(element => element.textContent === addonName)[0];
  ok(name, `Found ${addonName} add-on in the list`);
  const targetElement = name.parentNode.parentNode;
  const reloadButton = targetElement.querySelector(".reload-button");
  info(`Found reload button for ${addonName}`);
  return reloadButton;
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
  yield installAddon(document, "addons/unpacked/install.rdf",
                     ADDON_NAME, ADDON_NAME);

  const reloadButton = getReloadButton(document, ADDON_NAME);
  const onInstalled = promiseAddonEvent("onInstalled");

  const onBootstrapInstallCalled = new Promise(done => {
    Services.obs.addObserver(function listener() {
      Services.obs.removeObserver(listener, ADDON_NAME, false);
      info("Add-on was re-installed: " + ADDON_NAME);
      done();
    }, ADDON_NAME, false);
  });

  reloadButton.click();

  const [reloadedAddon] = yield onInstalled;
  is(reloadedAddon.name, ADDON_NAME,
     "Add-on was reloaded: " + reloadedAddon.name);

  yield onBootstrapInstallCalled;

  info("Uninstall addon installed earlier.");
  const onUninstalled = promiseAddonEvent("onUninstalled");
  reloadedAddon.uninstall();
  const [uninstalledAddon] = yield onUninstalled;
  is(uninstalledAddon.id, ADDON_ID,
     "Add-on was uninstalled: " + uninstalledAddon.id);

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

  const onAddonListUpdated = waitForMutation(getAddonList(document),
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
  const onAddonReloaded = waitForMutation(getAddonList(document),
                                          { childList: true, subtree: true });
  const reloadButton = getReloadButton(document, manifestBase.name);
  reloadButton.click();

  yield onAddonReloaded;
  const [reloadedAddon] = yield onReInstall;
  // Make sure the name was updated correctly.
  const allAddons = [...document.querySelectorAll("#addons .target-name")]
    .map(element => element.textContent);
  const nameWasUpdated = allAddons.some(name => name === newName);
  ok(nameWasUpdated, `New name appeared in reloaded add-ons: ${allAddons}`);

  const onUninstalled = promiseAddonEvent("onUninstalled");
  reloadedAddon.uninstall();
  yield onUninstalled;

  tempExt.remove();
  yield closeAboutDebugging(tab);
});
