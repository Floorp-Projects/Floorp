/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  checkVersionCompatibility,
} = require("devtools/client/shared/remote-debugging/version-checker");

const { RUNTIME_PREFERENCE } = require("../constants");
const { WorkersListener } = require("devtools/client/shared/workers-listener");

const PREF_TYPES = {
  BOOL: "BOOL",
};

// Map of preference to preference type.
const PREF_TO_TYPE = {
  [RUNTIME_PREFERENCE.CHROME_DEBUG_ENABLED]: PREF_TYPES.BOOL,
  [RUNTIME_PREFERENCE.CONNECTION_PROMPT]: PREF_TYPES.BOOL,
  [RUNTIME_PREFERENCE.PERMANENT_PRIVATE_BROWSING]: PREF_TYPES.BOOL,
  [RUNTIME_PREFERENCE.REMOTE_DEBUG_ENABLED]: PREF_TYPES.BOOL,
  [RUNTIME_PREFERENCE.SERVICE_WORKERS_ENABLED]: PREF_TYPES.BOOL,
};

// Some events are fired by mainRoot rather than client.
const MAIN_ROOT_EVENTS = ["addonListChanged", "tabListChanged"];

/**
 * The ClientWrapper class is used to isolate aboutdebugging from the DevTools client API
 * The modules of about:debugging should never call DevTools client APIs directly.
 */
class ClientWrapper {
  constructor(client) {
    this.client = client;
    this.workersListener = new WorkersListener(client.mainRoot);
  }

  once(evt, listener) {
    if (MAIN_ROOT_EVENTS.includes(evt)) {
      this.client.mainRoot.once(evt, listener);
    } else {
      this.client.once(evt, listener);
    }
  }

  on(evt, listener) {
    if (evt === "workersUpdated") {
      this.workersListener.addListener(listener);
    } else if (MAIN_ROOT_EVENTS.includes(evt)) {
      this.client.mainRoot.on(evt, listener);
    } else {
      this.client.on(evt, listener);
    }
  }

  off(evt, listener) {
    if (evt === "workersUpdated") {
      this.workersListener.removeListener(listener);
    } else if (MAIN_ROOT_EVENTS.includes(evt)) {
      this.client.mainRoot.off(evt, listener);
    } else {
      this.client.off(evt, listener);
    }
  }

  async getFront(typeName) {
    return this.client.mainRoot.getFront(typeName);
  }

  onFront(typeName, listener) {
    this.client.mainRoot.onFront(typeName, listener);
  }

  async getDeviceDescription() {
    const deviceFront = await this.getFront("device");
    const description = await deviceFront.getDescription();

    // Only expose a specific set of properties.
    return {
      canDebugServiceWorkers: description.canDebugServiceWorkers,
      channel: description.channel,
      deviceName: description.deviceName,
      name: description.brandName,
      os: description.os,
      version: description.version,
    };
  }

  async checkVersionCompatibility() {
    return checkVersionCompatibility(this.client);
  }

  async setPreference(prefName, value) {
    const prefType = PREF_TO_TYPE[prefName];
    const preferenceFront = await this.client.mainRoot.getFront("preference");
    switch (prefType) {
      case PREF_TYPES.BOOL:
        return preferenceFront.setBoolPref(prefName, value);
      default:
        throw new Error("Unsupported preference" + prefName);
    }
  }

  async getPreference(prefName, defaultValue) {
    if (typeof defaultValue === "undefined") {
      throw new Error(
        "Default value is mandatory for getPreference, the actor will " +
          "throw if the preference is not set on the target runtime"
      );
    }

    const prefType = PREF_TO_TYPE[prefName];
    const preferenceFront = await this.client.mainRoot.getFront("preference");
    switch (prefType) {
      case PREF_TYPES.BOOL:
        // TODO: Add server-side trait and methods to pass a default value to getBoolPref.
        // See Bug 1522588.
        let prefValue;
        try {
          prefValue = await preferenceFront.getBoolPref(prefName);
        } catch (e) {
          prefValue = defaultValue;
        }
        return prefValue;
      default:
        throw new Error("Unsupported preference:" + prefName);
    }
  }

  async listTabs(options) {
    return this.client.mainRoot.listTabs(options);
  }

  async listAddons(options) {
    return this.client.mainRoot.listAddons(options);
  }

  async getAddon({ id }) {
    return this.client.mainRoot.getAddon({ id });
  }

  async getMainProcess() {
    return this.client.mainRoot.getMainProcess();
  }

  async getServiceWorkerFront({ id }) {
    return this.client.mainRoot.getWorker(id);
  }

  async listWorkers() {
    const {
      other,
      service,
      shared,
    } = await this.client.mainRoot.listAllWorkers();

    return {
      otherWorkers: other,
      serviceWorkers: service,
      sharedWorkers: shared,
    };
  }

  async close() {
    return this.client.close();
  }

  isClosed() {
    return this.client._closed;
  }

  // This method will be mocked to return a dummy URL during mochitests
  getPerformancePanelUrl() {
    return "chrome://devtools/content/performance-new/index.xhtml";
  }

  async loadPerformanceProfiler(win) {
    const preferenceFront = await this.getFront("preference");
    const perfFront = await this.getFront("perf");
    const perfActorVersion = this.client.mainRoot.traits.perfActorVersion;
    win.gInit(perfFront, preferenceFront, perfActorVersion);
  }
}

exports.ClientWrapper = ClientWrapper;
