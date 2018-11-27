/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { RUNTIME_PREFERENCE } = require("../constants");

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
  "processListChanged",
  "serviceWorkerRegistrationListChanged",
  "tabListChanged",
  "workerListChanged",
];

/**
 * The ClientWrapper class is used to isolate aboutdebugging from the DevTools client API
 * The modules of about:debugging should never call DevTools client APIs directly.
 */
class ClientWrapper {
  constructor(client) {
    this.client = client;
  }

  addOneTimeListener(evt, listener) {
    if (MAIN_ROOT_EVENTS.includes(evt)) {
      this.client.mainRoot.once(evt, listener);
    } else {
      this.client.addOneTimeListener(evt, listener);
    }
  }

  addListener(evt, listener) {
    if (MAIN_ROOT_EVENTS.includes(evt)) {
      this.client.mainRoot.on(evt, listener);
    } else {
      this.client.addListener(evt, listener);
    }
  }

  removeListener(evt, listener) {
    if (MAIN_ROOT_EVENTS.includes(evt)) {
      this.client.mainRoot.off(evt, listener);
    } else {
      this.client.removeListener(evt, listener);
    }
  }

  async getDeviceDescription() {
    const deviceFront = await this.client.mainRoot.getFront("device");
    const { brandName, channel, deviceName, version } =
      await deviceFront.getDescription();
    // Only expose a specific set of properties.
    return {
      channel,
      deviceName,
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
    return this.client.listTabs(options);
  }

  async listAddons() {
    return this.client.mainRoot.listAddons();
  }

  async getAddon({ id }) {
    return this.client.mainRoot.getAddon({ id });
  }

  async listWorkers() {
    const { other, service, shared } = await this.client.mainRoot.listAllWorkers();

    return {
      otherWorkers: other,
      serviceWorkers: service,
      sharedWorkers: shared,
    };
  }

  async request(options) {
    return this.client.request(options);
  }

  async close() {
    return this.client.close();
  }
}

exports.ClientWrapper = ClientWrapper;
