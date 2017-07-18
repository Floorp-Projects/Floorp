/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionPreferencesManager",
                                  "resource://gre/modules/ExtensionPreferencesManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");

const LOCAL_STORE_TYPE = "dynamic_values";
const DEFAULT_SEARCH_SETTING_NAME = "defaultSearch";

const processDefaultSearchSetting = (extension, action) => {
  let item = ExtensionSettingsStore[action](extension, LOCAL_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME);
  if (["disable", "removeSetting"].includes(action) && item) {
    try {
      let engine = Services.search.getEngineByName(item.value || item.initialValue);
      if (engine && Services.search.getDefaultEngines().includes(engine)) {
        Services.search.currentEngine = engine;
      }
    } catch (e) {
      Components.utils.reportError(e);
    }
  }
};

const searchInitialized = () => {
  return new Promise(resolve => {
    if (Services.search.isInitialized) {
      resolve();
    }
    const SEARCH_SERVICE_TOPIC = "browser-search-service";
    Services.obs.addObserver(function observer(subject, topic, data) {
      if (data != "init-complete") {
        return;
      }

      Services.obs.removeObserver(observer, SEARCH_SERVICE_TOPIC);
      resolve();
    }, SEARCH_SERVICE_TOPIC);
  });
};

/* eslint-disable mozilla/balanced-listeners */
Management.on("shutdown", async (type, extension) => {
  switch (extension.shutdownReason) {
    case "ADDON_DISABLE":
    case "ADDON_DOWNGRADE":
    case "ADDON_UPGRADE":
      await searchInitialized();
      processDefaultSearchSetting(extension, "disable");
      break;

    case "ADDON_UNINSTALL":
      await searchInitialized();
      processDefaultSearchSetting(extension, "removeSetting");
      break;
  }
});

Management.on("startup", async (type, extension) => {
  if (["ADDON_ENABLE", "ADDON_UPGRADE", "ADDON_DOWNGRADE"].includes(extension.startupReason)) {
    await searchInitialized();
    processDefaultSearchSetting(extension, "enable");
  }
});
/* eslint-enable mozilla/balanced-listeners */

this.chrome_settings_overrides = class extends ExtensionAPI {
  async onManifestEntry(entryName) {
    let {extension} = this;
    let {manifest} = extension;

    if (manifest.chrome_settings_overrides.homepage) {
      ExtensionPreferencesManager.setSetting(extension, "homepage_override",
                                             manifest.chrome_settings_overrides.homepage);
    }
    if (manifest.chrome_settings_overrides.search_provider) {
      let setDefault = false;
      await searchInitialized();
      let searchProvider = manifest.chrome_settings_overrides.search_provider;
      if (searchProvider.is_default) {
        let engineName = searchProvider.name.trim();
        let engine = Services.search.getEngineByName(engineName);
        if (engine && Services.search.getDefaultEngines().includes(engine)) {
          await ExtensionSettingsStore.addSetting(
            extension, LOCAL_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME, engineName, () => {
              return Services.search.currentEngine.name;
            });
          setDefault = true;
        } else {
          Components.utils.reportError("is_default can only be used for built-in engines.")
        }
      // If the setting exists for the extension, but is missing from the manifest,
      // remove it
      } else if (ExtensionSettingsStore.hasSetting(
                 extension, LOCAL_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME)) {
        await searchInitialized();
        processDefaultSearchSetting(extension, "removeSetting");
      }
      // We only process the rest of search_provider if we did not set
      // the default engine
      if (!setDefault) {
        let isCurrent = false;
        let index = -1;
        if (extension.startupReason === "ADDON_UPGRADE") {
          let engines = Services.search.getEnginesByExtensionID(extension.id);
          if (engines.length > 0) {
            // There can be only one engine right now
            isCurrent = Services.search.currentEngine == engines[0];
            // Get position of engine and store it
            index = Services.search.getEngines().indexOf(engines[0]);
            Services.search.removeEngine(engines[0]);
          }
        }
        try {
          Services.search.addEngineWithDetails(searchProvider.name.trim(),
                                               searchProvider.favicon_url,
                                               searchProvider.keyword, null,
                                               "GET", searchProvider.search_url,
                                               extension.id);
          if (extension.startupReason === "ADDON_UPGRADE") {
            let engine = Services.search.getEngineByName(searchProvider.name.trim());
            if (isCurrent) {
              Services.search.currentEngine = engine;
            }
            if (index != -1) {
              Services.search.moveEngine(engine, index);
            }
          }
        } catch (e) {
          Components.utils.reportError(e);
        }
      }
    }
    let item = ExtensionSettingsStore.getSetting(LOCAL_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME);
    if (item) {
      await searchInitialized();
      Services.search.currentEngine = Services.search.getEngineByName(item.value || item.initialValue);
    }
  }
  async onShutdown(reason) {
    let {extension} = this;
    if (reason == "ADDON_DISABLE" ||
        reason == "ADDON_UNINSTALL") {
      if (extension.manifest.chrome_settings_overrides.search_provider) {
        await searchInitialized();
        let engines = Services.search.getEnginesByExtensionID(extension.id);
        for (let engine of engines) {
          try {
            Services.search.removeEngine(engine);
          } catch (e) {
            Components.utils.reportError(e);
          }
        }
      }
    }
  }
};

ExtensionPreferencesManager.addSetting("homepage_override", {
  prefNames: [
    "browser.startup.homepage",
  ],
  setCallback(value) {
    return {
      "browser.startup.homepage": value,
    };
  },
});
