/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * This module provides an interface to access DoH configuration - e.g. whether
 * DoH is enabled, whether capabilities are enabled, etc. The configuration is
 * sourced from either Remote Settings or pref values, with Remote Settings
 * being preferred.
 */
var EXPORTED_SYMBOLS = ["DoHConfigController"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Preferences: "resource://gre/modules/Preferences.jsm",
  Region: "resource://gre/modules/Region.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  Services: "resource://gre/modules/Services.jsm",
});

const kGlobalPrefBranch = "doh-rollout";
var kRegionPrefBranch;

const kConfigPrefs = {
  kEnabledPref: "enabled",
  kProvidersPref: "provider-list",
  kTRRSelectionEnabledPref: "trr-selection.enabled",
  kTRRSelectionProvidersPref: "trr-selection.provider-list",
  kTRRSelectionCommitResultPref: "trr-selection.commit-result",
  kProviderSteeringEnabledPref: "provider-steering.enabled",
  kProviderSteeringListPref: "provider-steering.provider-list",
};

const kPrefChangedTopic = "nsPref:changed";

const gProvidersCollection = RemoteSettings("doh-providers");
const gConfigCollection = RemoteSettings("doh-config");

function getPrefValueRegionFirst(prefName) {
  let regionalPrefName = `${kRegionPrefBranch}.${prefName}`;
  let regionalPrefValue = Preferences.get(regionalPrefName);
  if (regionalPrefValue !== undefined) {
    return regionalPrefValue;
  }
  return Preferences.get(`${kGlobalPrefBranch}.${prefName}`);
}

function getProviderListFromPref(prefName) {
  let prefVal = getPrefValueRegionFirst(prefName);
  if (prefVal) {
    try {
      return JSON.parse(prefVal);
    } catch (e) {
      Cu.reportError(`DoH provider list not a valid JSON array: ${prefName}`);
    }
  }
  return undefined;
}

// Generate a base config object with getters that return pref values. When
// Remote Settings values become available, a new config object will be
// generated from this and specific fields will be replaced by the RS value.
// If we use a class to store base config and instantiate new config objects
// from it, we lose the ability to override getters because they are defined
// as non-configureable properties on class instances. So just use a function.
function makeBaseConfigObject() {
  function makeConfigProperty({
    obj,
    propName,
    defaultVal,
    prefName,
    isProviderList,
  }) {
    let prefFn = isProviderList
      ? getProviderListFromPref
      : getPrefValueRegionFirst;

    let overridePropName = "_" + propName;

    Object.defineProperty(obj, propName, {
      get() {
        // If a pref value exists, it gets top priority. Otherwise, if it has an
        // explicitly set value (from Remote Settings), we return that.
        let prefVal = prefFn(prefName);
        if (prefVal !== undefined) {
          return prefVal;
        }
        if (this[overridePropName] !== undefined) {
          return this[overridePropName];
        }
        return defaultVal;
      },
      set(val) {
        this[overridePropName] = val;
      },
    });
  }
  let newConfig = {
    get fallbackProviderURI() {
      return this.providerList[0]?.uri;
    },
    trrSelection: {},
    providerSteering: {},
  };
  makeConfigProperty({
    obj: newConfig,
    propName: "enabled",
    defaultVal: false,
    prefName: kConfigPrefs.kEnabledPref,
    isProviderList: false,
  });
  makeConfigProperty({
    obj: newConfig,
    propName: "providerList",
    defaultVal: [],
    prefName: kConfigPrefs.kProvidersPref,
    isProviderList: true,
  });
  makeConfigProperty({
    obj: newConfig.trrSelection,
    propName: "enabled",
    defaultVal: false,
    prefName: kConfigPrefs.kTRRSelectionEnabledPref,
    isProviderList: false,
  });
  makeConfigProperty({
    obj: newConfig.trrSelection,
    propName: "commitResult",
    defaultVal: false,
    prefName: kConfigPrefs.kTRRSelectionCommitResultPref,
    isProviderList: false,
  });
  makeConfigProperty({
    obj: newConfig.trrSelection,
    propName: "providerList",
    defaultVal: [],
    prefName: kConfigPrefs.kTRRSelectionProvidersPref,
    isProviderList: true,
  });
  makeConfigProperty({
    obj: newConfig.providerSteering,
    propName: "enabled",
    defaultVal: false,
    prefName: kConfigPrefs.kProviderSteeringEnabledPref,
    isProviderList: false,
  });
  makeConfigProperty({
    obj: newConfig.providerSteering,
    propName: "providerList",
    defaultVal: [],
    prefName: kConfigPrefs.kProviderSteeringListPref,
    isProviderList: true,
  });
  return newConfig;
}

const DoHConfigController = {
  initComplete: null,
  _resolveInitComplete: null,

  // This field always contains the current config state, for
  // consumer use.
  currentConfig: makeBaseConfigObject(),

  // Loads the client's region via Region.jsm. This might mean waiting
  // until the region is available.
  async loadRegion() {
    await new Promise(resolve => {
      let homeRegion = Preferences.get(`${kGlobalPrefBranch}.home-region`);
      if (homeRegion) {
        kRegionPrefBranch = `${kGlobalPrefBranch}.${homeRegion.toLowerCase()}`;
        resolve();
        return;
      }

      let updateRegionAndResolve = () => {
        kRegionPrefBranch = `${kGlobalPrefBranch}.${Region.home.toLowerCase()}`;
        Preferences.set(`${kGlobalPrefBranch}.home-region`, Region.home);
        resolve();
      };

      if (Region.home) {
        updateRegionAndResolve();
        return;
      }

      Services.obs.addObserver(function obs(sub, top, data) {
        Services.obs.removeObserver(obs, Region.REGION_TOPIC);
        updateRegionAndResolve();
      }, Region.REGION_TOPIC);
    });

    // Finally, reload config.
    await this.updateFromRemoteSettings();
  },

  async init() {
    await this.loadRegion();

    Services.prefs.addObserver(`${kGlobalPrefBranch}.`, this, true);

    gProvidersCollection.on("sync", this.updateFromRemoteSettings);
    gConfigCollection.on("sync", this.updateFromRemoteSettings);

    this._resolveInitComplete();
  },

  // Useful for tests to set prior state before init()
  async _uninit() {
    await this.initComplete;

    Services.prefs.removeObserver(`${kGlobalPrefBranch}`, this);

    gProvidersCollection.off("sync", this.updateFromRemoteSettings);
    gConfigCollection.off("sync", this.updateFromRemoteSettings);

    this.initComplete = new Promise(resolve => {
      this._resolveInitComplete = resolve;
    });
  },

  observe(subject, topic, data) {
    switch (topic) {
      case kPrefChangedTopic:
        let allowedPrefs = Object.getOwnPropertyNames(kConfigPrefs).map(
          k => kConfigPrefs[k]
        );
        if (
          !allowedPrefs.some(pref =>
            [
              `${kRegionPrefBranch}.${pref}`,
              `${kGlobalPrefBranch}.${pref}`,
            ].includes(data)
          )
        ) {
          break;
        }
        this.notifyNewConfig();
        break;
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  // Creates new config object from currently available
  // Remote Settings values.
  async updateFromRemoteSettings() {
    let providers = await gProvidersCollection.get();
    let config = await gConfigCollection.get();

    let providersById = new Map();
    providers.forEach(p => providersById.set(p.id, p));

    let configByRegion = new Map();
    config.forEach(c => {
      c.id = c.id.toLowerCase();
      configByRegion.set(c.id, c);
    });

    let homeRegion = Preferences.get(`${kGlobalPrefBranch}.home-region`);
    let localConfig =
      configByRegion.get(homeRegion?.toLowerCase()) ||
      configByRegion.get("global");

    // Make a new config object first, mutate it as needed, then synchronously
    // replace the currentConfig object at the end to ensure atomicity.
    let newConfig = makeBaseConfigObject();

    if (!localConfig) {
      DoHConfigController.currentConfig = newConfig;
      DoHConfigController.notifyNewConfig();
      return;
    }

    if (localConfig.rolloutEnabled) {
      newConfig.enabled = true;
    }

    let parseProviderList = (list, checkFn) => {
      let parsedList = [];
      list?.split(",")?.forEach(p => {
        p = p.trim();
        if (!p.length) {
          return;
        }
        p = providersById.get(p);
        if (!p || (checkFn && !checkFn(p))) {
          return;
        }
        parsedList.push(p);
      });
      return parsedList;
    };

    let regionalProviders = parseProviderList(localConfig.providers);
    if (regionalProviders?.length) {
      newConfig.providerList = regionalProviders;
    }

    if (localConfig.steeringEnabled) {
      let steeringProviders = parseProviderList(
        localConfig.steeringProviders,
        p => p.canonicalName?.length
      );
      if (steeringProviders?.length) {
        newConfig.providerSteering.providerList = steeringProviders;
        newConfig.providerSteering.enabled = true;
      }
    }

    if (localConfig.autoDefaultEnabled) {
      let defaultProviders = parseProviderList(
        localConfig.autoDefaultProviders
      );
      if (defaultProviders?.length) {
        newConfig.trrSelection.providerList = defaultProviders;
        newConfig.trrSelection.enabled = true;
      }
    }

    // Finally, update the currentConfig object synchronously.
    DoHConfigController.currentConfig = newConfig;

    DoHConfigController.notifyNewConfig();
  },

  kConfigUpdateTopic: "doh-config-updated",
  notifyNewConfig() {
    Services.obs.notifyObservers(null, this.kConfigUpdateTopic);
  },
};

DoHConfigController.initComplete = new Promise(resolve => {
  DoHConfigController._resolveInitComplete = resolve;
});
DoHConfigController.init();
