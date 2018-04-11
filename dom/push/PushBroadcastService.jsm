/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/osfile.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.defineModuleGetter(this, "JSONFile", "resource://gre/modules/JSONFile.jsm");

var EXPORTED_SYMBOLS = ["pushBroadcastService"];

// We are supposed to ignore any updates with this version.
// FIXME: what is the actual "dummy" version?
// See bug 1467550.
const DUMMY_VERSION_STRING = "dummy";

XPCOMUtils.defineLazyGetter(this, "console", () => {
  let {ConsoleAPI} = ChromeUtils.import("resource://gre/modules/Console.jsm", {});
  return new ConsoleAPI({
    maxLogLevelPref: "dom.push.loglevel",
    prefix: "BroadcastService",
  });
});
ChromeUtils.defineModuleGetter(this, "PushService", "resource://gre/modules/PushService.jsm");

class InvalidSourceInfo extends Error {
  constructor(message) {
    super(message);
    this.name = 'InvalidSourceInfo';
  }
}

const BROADCAST_SERVICE_VERSION = 1;

var BroadcastService = class {
  constructor(pushService, path) {
    this.pushService = pushService;
    this.jsonFile = new JSONFile({
      path,
      dataPostProcessor: this._initializeJSONFile,
    });
    this.initializePromise = this.jsonFile.load();
  }

  /**
   * Convert the listeners from our on-disk format to the format
   * needed by a hello message.
   */
  async getListeners() {
    await this.initializePromise;
    return Object.entries(this.jsonFile.data.listeners).reduce((acc, [k, v]) => {
      acc[k] = v.version;
      return acc;
    }, {});
  }

  _initializeJSONFile(data) {
    if (!data.version) {
      data.version = BROADCAST_SERVICE_VERSION;
    }
    if (!data.hasOwnProperty("listeners")) {
      data.listeners = {};
    }
    return data;
  }

  /**
   * Reset to a state akin to what you would get in a new profile.
   * In particular, wipe anything from storage.
   *
   * Used mainly for testing.
   */
  async _resetListeners() {
    await this.initializePromise;
    this.jsonFile.data = this._initializeJSONFile({});
    this.initializePromise = Promise.resolve();
  }

  /**
   * Ensure that a sourceInfo is correct (has the expected fields).
   */
  _validateSourceInfo(sourceInfo) {
    const {moduleURI, symbolName} = sourceInfo;
    if (typeof moduleURI !== "string") {
      throw new InvalidSourceInfo(`moduleURI must be a string (got ${typeof moduleURI})`);
    }
    if (typeof symbolName !== "string") {
      throw new InvalidSourceInfo(`symbolName must be a string (got ${typeof symbolName})`);
    }
  }

  /**
   * Add an entry for a given listener if it isn't present, or update
   * one if it is already present.
   *
   * Note that this means only a single listener can be set for a
   * given subscription. This is a limitation in the current API that
   * stems from the fact that there can only be one source of truth
   * for the subscriber's version. As a workaround, you can define a
   * listener which calls multiple other listeners.
   *
   * @param {string} broadcastId The broadcastID to listen for
   * @param {string} version The most recent version we have for
   *   updates from this broadcastID
   * @param {Object} sourceInfo A description of the handler for
   *   updates on this broadcastID
   */
  async addListener(broadcastId, version, sourceInfo) {
    console.info("addListener: adding listener", broadcastId, version, sourceInfo);
    await this.initializePromise;
    this._validateSourceInfo(sourceInfo);
    if (typeof version !== "string") {
      throw new TypeError("version should be a string");
    }
    const isNew = !this.jsonFile.data.listeners.hasOwnProperty(broadcastId);

    // Update listeners before telling the pushService to subscribe,
    // in case it would disregard the update in the small window
    // between getting listeners and setting state to RUNNING.
    this.jsonFile.data.listeners[broadcastId] = {version, sourceInfo};
    this.jsonFile.saveSoon();

    if (isNew) {
      await this.pushService.subscribeBroadcast(broadcastId, version);
    }
  }

  async receivedBroadcastMessage(broadcasts) {
    console.info("receivedBroadcastMessage:", broadcasts);
    await this.initializePromise;
    for (const broadcastId in broadcasts) {
      const version = broadcasts[broadcastId];
      if (version === DUMMY_VERSION_STRING) {
        console.info("Ignoring", version, "because it's the dummy version");
        continue;
      }
      // We don't know this broadcastID. This is probably a bug?
      if (!this.jsonFile.data.listeners.hasOwnProperty(broadcastId)) {
        console.warn("receivedBroadcastMessage: unknown broadcastId", broadcastId);
        continue;
      }

      const {sourceInfo} = this.jsonFile.data.listeners[broadcastId];
      try {
        this._validateSourceInfo(sourceInfo);
      } catch (e) {
        console.error("receivedBroadcastMessage: malformed sourceInfo", sourceInfo, e);
        continue;
      }

      const {moduleURI, symbolName} = sourceInfo;

      const module = {};
      try {
        ChromeUtils.import(moduleURI, module);
      } catch (e) {
        console.error("receivedBroadcastMessage: couldn't invoke", broadcastId,
                      "because import of module", moduleURI,
                      "failed", e);
        continue;
      }

      if (!module[symbolName]) {
        console.error("receivedBroadcastMessage: couldn't invoke", broadcastId,
                      "because module", moduleName, "missing attribute", symbolName);
        continue;
      }

      const handler = module[symbolName];

      if (!handler.receivedBroadcastMessage) {
        console.error("receivedBroadcastMessage: couldn't invoke", broadcastId,
                      "because handler returned by", `${moduleURI}.${symbolName}`,
                      "has no receivedBroadcastMessage method");
        continue;
      }

      try {
        await handler.receivedBroadcastMessage(version, broadcastId);
      } catch (e) {
        console.error("receivedBroadcastMessage: handler for", broadcastId,
                      "threw error:", e);
        continue;
      }

      // Broadcast message applied successfully. Update the version we
      // received if it's different than the one we had.  We don't
      // enforce an ordering here (i.e. we use != instead of <)
      // because we don't know what the ordering of the service's
      // versions is going to be.
      if (this.jsonFile.data.listeners[broadcastId].version != version) {
        this.jsonFile.data.listeners[broadcastId].version = version;
        this.jsonFile.saveSoon();
      }
    }
  }

  // For test only.
  _saveImmediately() {
    return this.jsonFile._save();
  }
}

function initializeBroadcastService() {
  // Fallback path for xpcshell tests.
  let path = "broadcast-listeners.json";
  if (OS.Constants.Path.profileDir) {
    // Real path for use in a real profile.
    path = OS.Path.join(OS.Constants.Path.profileDir, path);
  }
  return new BroadcastService(PushService, path);
};

var pushBroadcastService = initializeBroadcastService();
