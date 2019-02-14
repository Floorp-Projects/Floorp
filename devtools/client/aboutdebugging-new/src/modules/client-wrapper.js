/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { RUNTIME_PREFERENCE } = require("../constants");
const { WorkersListener } = require("./workers-listener");

const PREF_TYPES = {
  BOOL: "BOOL",
};

// Map of preference to preference type.
const PREF_TO_TYPE = {
  [RUNTIME_PREFERENCE.CONNECTION_PROMPT]: PREF_TYPES.BOOL,
};

// Some events are fired by mainRoot rather than client.
const MAIN_ROOT_EVENTS = [
  "addonListChanged",
  "tabListChanged",
];

/**
 * The ClientWrapper class is used to isolate aboutdebugging from the DevTools client API
 * The modules of about:debugging should never call DevTools client APIs directly.
 */
class ClientWrapper {
  constructor(client) {
    this.client = client;
    this.workersListener = new WorkersListener(client.mainRoot);
  }

  addOneTimeListener(evt, listener) {
    if (MAIN_ROOT_EVENTS.includes(evt)) {
      this.client.mainRoot.once(evt, listener);
    } else {
      this.client.addOneTimeListener(evt, listener);
    }
  }

  addListener(evt, listener) {
    if (evt === "workersUpdated") {
      this.workersListener.addListener(listener);
    } else if (MAIN_ROOT_EVENTS.includes(evt)) {
      this.client.mainRoot.on(evt, listener);
    } else {
      this.client.addListener(evt, listener);
    }
  }

  removeListener(evt, listener) {
    if (evt === "workersUpdated") {
      this.workersListener.removeListener(listener);
    } else if (MAIN_ROOT_EVENTS.includes(evt)) {
      this.client.mainRoot.off(evt, listener);
    } else {
      this.client.removeListener(evt, listener);
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
    const { brandName, channel, deviceName, isMultiE10s, version } =
      await deviceFront.getDescription();
    // Only expose a specific set of properties.
    return {
      channel,
      deviceName,
      isMultiE10s,
      name: brandName,
      version,
    };
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

  async getPreference(prefName) {
    const prefType = PREF_TO_TYPE[prefName];
    const preferenceFront = await this.client.mainRoot.getFront("preference");
    switch (prefType) {
      case PREF_TYPES.BOOL:
        return preferenceFront.getBoolPref(prefName);
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

  async getServiceWorkerFront({ id }) {
    const { serviceWorkers } = await this.listWorkers();
    const workerFronts = serviceWorkers.map(sw => sw.workerTargetFront);
    return workerFronts.find(front => front && front.actorID === id);
  }

  async listWorkers() {
    const { other, service, shared } = await this.client.mainRoot.listAllWorkers();

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
}

exports.ClientWrapper = ClientWrapper;
