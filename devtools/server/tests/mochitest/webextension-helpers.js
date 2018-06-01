/* exported attachAddon, setWebExtensionOOPMode, waitForFramesUpdated, reloadAddon,
            collectFrameUpdates, generateWebExtensionXPI, promiseInstallFile,
            promiseAddonByID, promiseWebExtensionStartup, promiseWebExtensionShutdown
 */

"use strict";

const {require, loader} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const {DebuggerClient} = require("devtools/shared/client/debugger-client");
const {DebuggerServer} = require("devtools/server/main");
const {TargetFactory} = require("devtools/client/framework/target");

const {AddonManager} = require("resource://gre/modules/AddonManager.jsm");
const {Extension, Management} = require("resource://gre/modules/Extension.jsm");
const {flushJarCache} = require("resource://gre/modules/ExtensionUtils.jsm");
const {Services} = require("resource://gre/modules/Services.jsm");

loader.lazyImporter(this, "ExtensionParent", "resource://gre/modules/ExtensionParent.jsm");
loader.lazyImporter(this, "OS", "resource://gre/modules/osfile.jsm");

// Initialize a minimal DebuggerServer and connect to the webextension addon actor.
if (!DebuggerServer.initialized) {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  SimpleTest.registerCleanupFunction(function() {
    DebuggerServer.destroy();
  });
}

SimpleTest.registerCleanupFunction(function() {
  const {hiddenXULWindow} = ExtensionParent.DebugUtils;
  const debugBrowserMapSize = ExtensionParent.DebugUtils.debugBrowserPromises.size;

  if (debugBrowserMapSize > 0) {
    is(debugBrowserMapSize, 0,
       "ExtensionParent DebugUtils debug browsers have not been released");
  }

  if (hiddenXULWindow) {
    ok(false, "ExtensionParent DebugUtils hiddenXULWindow has not been destroyed");
  }
});

// Test helpers related to the webextensions debugging RDP actors.

function setWebExtensionOOPMode(oopMode) {
  return SpecialPowers.pushPrefEnv({
    "set": [
      ["extensions.webextensions.remote", oopMode],
    ]
  });
}

function waitForFramesUpdated({client}, matchFn) {
  return new Promise(resolve => {
    const listener = (evt, data) => {
      if (typeof matchFn === "function" && !matchFn(data)) {
        return;
      } else if (!data.frames) {
        return;
      }

      client.removeListener("frameUpdate", listener);
      resolve(data.frames);
    };
    client.addListener("frameUpdate", listener);
  });
}

function collectFrameUpdates({client}, matchFn) {
  const collected = [];

  const listener = (evt, data) => {
    if (matchFn(data)) {
      collected.push(data);
    }
  };

  client.addListener("frameUpdate", listener);
  let unsubscribe = () => {
    unsubscribe = null;
    client.removeListener("frameUpdate", listener);
    return collected;
  };

  SimpleTest.registerCleanupFunction(function() {
    if (unsubscribe) {
      unsubscribe();
    }
  });

  return unsubscribe;
}

async function attachAddon(addonId) {
  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);

  await client.connect();

  const {addons} = await client.mainRoot.listAddons();
  const addonActor = addons.filter(actor => actor.id === addonId).pop();

  if (!addonActor) {
    client.close();
    throw new Error(`No WebExtension Actor found for ${addonId}`);
  }

  const addonTarget = await TargetFactory.forRemoteTab({
    form: addonActor,
    client,
    chrome: true,
    isTabActor: true,
  });

  return addonTarget;
}

async function reloadAddon({client}, addonId) {
  const {addons} = await client.mainRoot.listAddons();
  const addonActor = addons.filter(actor => actor.id === addonId).pop();

  if (!addonActor) {
    client.close();
    throw new Error(`No WebExtension Actor found for ${addonId}`);
  }

  await client.request({
    to: addonActor.actor,
    type: "reload",
  });
}

// Test helpers related to the AddonManager.

function generateWebExtensionXPI(extDetails) {
  const addonFile = Extension.generateXPI(extDetails);

  flushJarCache(addonFile.path);
  Services.ppmm.broadcastAsyncMessage("Extension:FlushJarCache",
                                      {path: addonFile.path});

  // Remove the file on cleanup if needed.
  SimpleTest.registerCleanupFunction(() => {
    flushJarCache(addonFile.path);
    Services.ppmm.broadcastAsyncMessage("Extension:FlushJarCache",
                                        {path: addonFile.path});

    if (addonFile.exists()) {
      OS.File.remove(addonFile.path);
    }
  });

  return addonFile;
}

function promiseCompleteInstall(install) {
  let listener;
  return new Promise((resolve, reject) => {
    listener = {
      onDownloadFailed: reject,
      onDownloadCancelled: reject,
      onInstallFailed: reject,
      onInstallCancelled: reject,
      onInstallEnded: resolve,
      onInstallPostponed: reject,
    };

    install.addListener(listener);
    install.install();
  }).then(() => {
    install.removeListener(listener);
    return install;
  });
}

function promiseInstallFile(file) {
  return AddonManager.getInstallForFile(file).then(install => {
    if (!install) {
      throw new Error(`No AddonInstall created for ${file.path}`);
    }

    if (install.state != AddonManager.STATE_DOWNLOADED) {
      throw new Error(`Expected file to be downloaded for install of ${file.path}`);
    }

    return promiseCompleteInstall(install);
  });
}

function promiseWebExtensionStartup() {
  return new Promise(resolve => {
    const listener = (evt, extension) => {
      Management.off("ready", listener);
      resolve(extension);
    };

    Management.on("ready", listener);
  });
}

function promiseWebExtensionShutdown() {
  return new Promise(resolve => {
    const listener = (event, extension) => {
      Management.off("shutdown", listener);
      resolve(extension);
    };

    Management.on("shutdown", listener);
  });
}
