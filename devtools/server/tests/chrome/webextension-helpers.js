/* exported attachAddon, waitForFramesUpdated, reloadAddon,
   collectFrameUpdates, generateWebExtensionXPI, promiseInstallFile,
   promiseWebExtensionStartup, promiseWebExtensionShutdown
 */

"use strict";

const { require, loader } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);
const { DevToolsClient } = require("devtools/client/devtools-client");
const { DevToolsServer } = require("devtools/server/devtools-server");

const {
  AddonTestUtils,
} = require("resource://testing-common/AddonTestUtils.jsm");
const {
  ExtensionTestCommon,
} = require("resource://testing-common/ExtensionTestCommon.jsm");

loader.lazyImporter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);

// Initialize a minimal DevToolsServer and connect to the webextension addon actor.
if (!DevToolsServer.initialized) {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  SimpleTest.registerCleanupFunction(function() {
    DevToolsServer.destroy();
  });
}

SimpleTest.registerCleanupFunction(function() {
  const { hiddenXULWindow } = ExtensionParent.DebugUtils;
  const debugBrowserMapSize =
    ExtensionParent.DebugUtils.debugBrowserPromises.size;

  if (debugBrowserMapSize > 0) {
    is(
      debugBrowserMapSize,
      0,
      "ExtensionParent DebugUtils debug browsers have not been released"
    );
  }

  if (hiddenXULWindow) {
    ok(
      false,
      "ExtensionParent DebugUtils hiddenXULWindow has not been destroyed"
    );
  }
});

// Test helpers related to the webextensions debugging RDP actors.

function waitForFramesUpdated(target, matchFn) {
  return new Promise(resolve => {
    const listener = data => {
      if (typeof matchFn === "function" && !matchFn(data)) {
        return;
      } else if (!data.frames) {
        return;
      }

      target.off("frameUpdate", listener);
      resolve(data.frames);
    };
    target.on("frameUpdate", listener);
  });
}

function collectFrameUpdates({ client }, matchFn) {
  const collected = [];

  const listener = data => {
    if (matchFn(data)) {
      collected.push(data);
    }
  };

  client.on("frameUpdate", listener);
  let unsubscribe = () => {
    unsubscribe = null;
    client.off("frameUpdate", listener);
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
  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);

  await client.connect();

  const addonDescriptor = await client.mainRoot.getAddon({ id: addonId });
  const addonTarget = await addonDescriptor.getTarget();

  if (!addonTarget) {
    client.close();
    throw new Error(`No WebExtension Actor found for ${addonId}`);
  }

  return addonTarget;
}

async function reloadAddon({ client }, addonId) {
  const addonTargetFront = await client.mainRoot.getAddon({ id: addonId });

  if (!addonTargetFront) {
    client.close();
    throw new Error(`No WebExtension Actor found for ${addonId}`);
  }

  await addonTargetFront.reload();
}

// Test helpers related to the AddonManager.

function generateWebExtensionXPI(extDetails) {
  return ExtensionTestCommon.generateXPI(extDetails);
}

let {
  promiseInstallFile,
  promiseWebExtensionStartup,
  promiseWebExtensionShutdown,
} = AddonTestUtils;
