/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

const { Management } = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});

function _getSupportsFile(path) {
  const cr = Cc["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Ci.nsIChromeRegistry);
  const uri = Services.io.newURI(CHROME_URL_ROOT + path);
  const fileurl = cr.convertChromeURL(uri);
  return fileurl.QueryInterface(Ci.nsIFileURL);
}

async function enableExtensionDebugging() {
  // Force enabling of addons debugging
  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.debugger.remote-enabled", true);
  // Disable security prompt
  await pushPref("devtools.debugger.prompt-connection", false);
  // Enable Browser toolbox test script execution via env variable
  await pushPref("devtools.browser-toolbox.allow-unsafe-script", true);
}
/* exported enableExtensionDebugging */

/**
 * Install a temporary extension at the provided path, with the provided name.
 * Will use a mock file picker to select the file.
 */
async function installTemporaryExtension(path, name, document) {
  // Mock the file picker to select a test addon
  prepareMockFilePicker(path);

  const onAddonInstalled = new Promise(done => {
    Management.on("startup", function listener(event, extension) {
      if (extension.name != name) {
        return;
      }

      Management.off("startup", listener);
      done();
    });
  });

  // Trigger the file picker by clicking on the button
  document.querySelector(".js-temporary-extension-install-button").click();

  info("Wait for addon to be installed");
  await onAddonInstalled;
}
/* exported installTemporaryExtension */

/**
 * Install a fake temporary extension just using the manifest information.
 * @return {TemporaryExtension} the temporary extension instance created
 */
async function installTemporaryExtensionFromManifest(manifest, document) {
  const addonId = manifest.applications.gecko.id;
  const temporaryExtension = new TemporaryExtension(addonId);
  temporaryExtension.writeManifest(manifest);
  registerCleanupFunction(() => temporaryExtension.remove(false));

  info("Install a temporary extension");
  await AddonManager.installTemporaryAddon(temporaryExtension.sourceDir);

  info("Wait until the corresponding debug target item appears");
  await waitUntil(() => findDebugTargetByText(manifest.name, document));

  return temporaryExtension;
}
/* exported installTemporaryExtensionFromManifest */

async function removeTemporaryExtension(name, document) {
  info(`Remove the temporary extension with name: '${name}'`);
  const temporaryExtensionItem = findDebugTargetByText(name, document);
  temporaryExtensionItem.querySelector(".js-temporary-extension-remove-button").click();

  info("Wait until the debug target item disappears");
  await waitUntil(() => !findDebugTargetByText(name, document));
}
/* exported removeTemporaryExtension */

async function removeExtension(id, name, document) {
  info("Retrieve the extension instance from the addon manager, and uninstall it");
  const extension = await AddonManager.getAddonByID(id);
  extension.uninstall();

  info("Wait until the addon disappears from about:debugging");
  await waitUntil(() => !findDebugTargetByText(name, document));
}
/* exported removeExtension */

function prepareMockFilePicker(path) {
  // Mock the file picker to select a test addon
  const MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  MockFilePicker.setFiles([_getSupportsFile(path).file]);
}
/* exported prepareMockFilePicker */

/**
 * Creates a web extension from scratch in a temporary location.
 * The object must be removed when you're finished working with it.
 */
class TemporaryExtension {
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
/* exported TemporaryExtension */

/**
 * Install an add-on using the AddonManager so it does not show up as temporary.
 */
function installRegularAddon(filePath) {
  const file = _getSupportsFile(filePath).file;
  return new Promise(async (resolve, reject) => {
    const install = await AddonManager.getInstallForFile(file);
    if (!install) {
      throw new Error(`An install was not created for ${filePath}`);
    }
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
/* exported installRegularAddon */
