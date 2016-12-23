/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* globals Components */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource:///modules/ShellService.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Timer.jsm"); /* globals setTimeout, clearTimeout */
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://shield-recipe-client/lib/Storage.jsm");
Cu.import("resource://shield-recipe-client/lib/Heartbeat.jsm");

const {generateUUID} = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

this.EXPORTED_SYMBOLS = ["NormandyDriver"];

const log = Log.repository.getLogger("extensions.shield-recipe-client");
const actionLog = Log.repository.getLogger("extensions.shield-recipe-client.actions");

this.NormandyDriver = function(sandboxManager, extraContext = {}) {
  if (!sandboxManager) {
    throw new Error("sandboxManager is required");
  }
  const {sandbox} = sandboxManager;

  return {
    testing: false,

    get locale() {
      return Cc["@mozilla.org/chrome/chrome-registry;1"]
        .getService(Ci.nsIXULChromeRegistry)
        .getSelectedLocale("browser");
    },

    log(message, level = "debug") {
      const levels = ["debug", "info", "warn", "error"];
      if (levels.indexOf(level) === -1) {
        throw new Error(`Invalid log level "${level}"`);
      }
      actionLog[level](message);
    },

    showHeartbeat(options) {
      log.info(`Showing heartbeat prompt "${options.message}"`);
      const aWindow = Services.wm.getMostRecentWindow("navigator:browser");

      if (!aWindow) {
        return sandbox.Promise.reject(new sandbox.Error("No window to show heartbeat in"));
      }

      const sandboxedDriver = Cu.cloneInto(this, sandbox, {cloneFunctions: true});
      const ee = new sandbox.EventEmitter(sandboxedDriver).wrappedJSObject;
      const internalOptions = Object.assign({}, options, {testing: this.testing});
      new Heartbeat(aWindow, ee, sandboxManager, internalOptions);
      return sandbox.Promise.resolve(ee);
    },

    saveHeartbeatFlow() {
      // no-op required by spec
    },

    client() {
      const appinfo = {
        version: Services.appinfo.version,
        channel: Services.appinfo.defaultUpdateChannel,
        isDefaultBrowser: ShellService.isDefaultBrowser() || null,
        searchEngine: null,
        syncSetup: Preferences.isSet("services.sync.username"),
        plugins: {},
        doNotTrack: Preferences.get("privacy.donottrackheader.enabled", false),
      };

      const searchEnginePromise = new Promise(resolve => {
        Services.search.init(rv => {
          if (Components.isSuccessCode(rv)) {
            appinfo.searchEngine = Services.search.defaultEngine.identifier;
          }
          resolve();
        });
      });

      const pluginsPromise = new Promise(resolve => {
        AddonManager.getAddonsByTypes(["plugin"], plugins => {
          plugins.forEach(plugin => appinfo.plugins[plugin.name] = plugin);
          resolve();
        });
      });

      return new sandbox.Promise(resolve => {
        Promise.all([searchEnginePromise, pluginsPromise]).then(() => {
          resolve(Cu.cloneInto(appinfo, sandbox));
        });
      });
    },

    uuid() {
      let ret = generateUUID().toString();
      ret = ret.slice(1, ret.length - 1);
      return ret;
    },

    createStorage(keyPrefix) {
      let storage;
      try {
        storage = Storage.makeStorage(keyPrefix, sandbox);
      } catch (e) {
        log.error(e.stack);
        throw e;
      }
      return storage;
    },

    location() {
      const location = Cu.cloneInto({countryCode: extraContext.country}, sandbox);
      return sandbox.Promise.resolve(location);
    },

    setTimeout(cb, time) {
      if (typeof cb !== "function") {
        throw new sandbox.Error(`setTimeout must be called with a function, got "${typeof cb}"`);
      }
      const token = setTimeout(() => {
        cb();
        sandboxManager.removeHold(`setTimeout-${token}`);
      }, time);
      sandboxManager.addHold(`setTimeout-${token}`);
      return Cu.cloneInto(token, sandbox);
    },

    clearTimeout(token) {
      clearTimeout(token);
      sandboxManager.removeHold(`setTimeout-${token}`);
    },
  };
};
