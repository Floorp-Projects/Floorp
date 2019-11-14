/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global browser */

// This tests the browser.experiments.urlbar.isBrowserUpdateReadyToInstall
// WebExtension Experiment API.

"use strict";

const mockUpdateManager = {
  contractId: "@mozilla.org/updates/update-manager;1",

  _mockClassId: Cc["@mozilla.org/uuid-generator;1"]
    .getService(Ci.nsIUUIDGenerator)
    .generateUUID(),
  _originalClassId: "",

  QueryInterface: ChromeUtils.generateQI([Ci.nsIUpdateManager]),

  createInstance(outer, iiD) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(iiD);
  },

  register() {
    let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
    if (!registrar.isCIDRegistered(this._mockClassId)) {
      this._originalClassId = registrar.contractIDToCID(this.contractId);
      registrar.registerFactory(
        this._mockClassId,
        "Unregister after testing",
        this.contractId,
        this
      );
    }
  },

  unregister() {
    let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(this._mockClassId, this);
    registrar.registerFactory(this._originalClassId, "", this.contractId, null);
  },

  setUpdateState(state, errorCode = 0) {
    this._state = state;
    this._errorCode = errorCode;
  },

  get activeUpdate() {
    return {
      type: "major",
      name: "Firefox Developer Edition 49.0a2",
      state: this._state,
      errorCode: this._errorCode,
    };
  },
};

add_task(async function noUpdate() {
  info("should return false when there are no updates");
  await checkExtension(false);
});

add_task(async function hasUpdate() {
  mockUpdateManager.register();
  registerCleanupFunction(() => {
    mockUpdateManager.unregister();
  });

  await SpecialPowers.pushPrefEnv({
    set: [["app.update.staging.enabled", true]],
  });

  info("Should return false when an update is not ready");
  for (let state of [
    "downloading",
    "applying",
    "succeeded",
    "download-failed",
    "failed",
  ]) {
    mockUpdateManager.setUpdateState(state);
    await checkExtension(false);
  }

  info("Should return false when an update is pending with no error");
  mockUpdateManager.setUpdateState("pending");
  await checkExtension(false);

  info("Should return true when an update is pending with error");
  mockUpdateManager.setUpdateState("pending", 1);
  await checkExtension(true);

  info("Should return true when an update was applied");
  for (let state of ["applied", "applied-service"]) {
    mockUpdateManager.setUpdateState(state);
    await checkExtension(true);
  }

  await SpecialPowers.pushPrefEnv({
    set: [["app.update.staging.enabled", false]],
  });

  info("Should return true when an update is pending and staging is disabled");
  mockUpdateManager.setUpdateState("pending");
  await checkExtension(true);
});

async function checkExtension(expectedReady) {
  let ext = await loadExtension(async () => {
    let ready = await browser.experiments.urlbar.isBrowserUpdateReadyToInstall();
    browser.test.sendMessage("ready", ready);
  });
  let ready = await ext.awaitMessage("ready");
  Assert.equal(ready, expectedReady);
  await ext.unload();
}
