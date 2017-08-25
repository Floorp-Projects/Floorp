/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionPreferencesManager",
                                  "resource://gre/modules/ExtensionPreferencesManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");

const DEFAULT_SEARCH_STORE_TYPE = "default_search";
const DEFAULT_SEARCH_SETTING_NAME = "defaultSearch";

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

this.chrome_settings_overrides = class extends ExtensionAPI {
  processDefaultSearchSetting(action) {
    let {extension} = this;
    let item = ExtensionSettingsStore.getSetting(DEFAULT_SEARCH_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME);
    if (!item) {
      return;
    }
    if (Services.search.currentEngine.name != item.value &&
        Services.search.currentEngine.name != item.initialValue) {
      // The current engine is not the same as the value that the ExtensionSettingsStore has.
      // This means that the user changed the engine, so we shouldn't control it anymore.
      // Do nothing and remove our entry from the ExtensionSettingsStore.
      ExtensionSettingsStore.removeSetting(extension, DEFAULT_SEARCH_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME);
      return;
    }
    item = ExtensionSettingsStore[action](extension, DEFAULT_SEARCH_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME);
    if (item) {
      try {
        let engine = Services.search.getEngineByName(item.value || item.initialValue);
        if (engine) {
          Services.search.currentEngine = engine;
        }
      } catch (e) {
        Components.utils.reportError(e);
      }
    }
  }

  async onManifestEntry(entryName) {
    let {extension} = this;
    let {manifest} = extension;

    await ExtensionSettingsStore.initialize();
    if (manifest.chrome_settings_overrides.homepage) {
      ExtensionPreferencesManager.setSetting(extension, "homepage_override",
                                             manifest.chrome_settings_overrides.homepage);
    }
    if (manifest.chrome_settings_overrides.search_provider) {
      await searchInitialized();
      let searchProvider = manifest.chrome_settings_overrides.search_provider;
      if (searchProvider.is_default) {
        let engineName = searchProvider.name.trim();
        let engine = Services.search.getEngineByName(engineName);
        if (engine && Services.search.getDefaultEngines().includes(engine)) {
          // Only add onclose handlers if we would definitely
          // be setting the default engine.
          extension.callOnClose({
            close: () => {
              switch (extension.shutdownReason) {
                case "ADDON_DISABLE":
                  this.processDefaultSearchSetting("disable");
                  break;

                case "ADDON_UNINSTALL":
                  this.processDefaultSearchSetting("removeSetting");
                  break;
              }
            },
          });
          if (extension.startupReason === "ADDON_INSTALL") {
            let item = await ExtensionSettingsStore.addSetting(
              extension, DEFAULT_SEARCH_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME, engineName, () => {
                return Services.search.currentEngine.name;
              });
            Services.search.currentEngine = Services.search.getEngineByName(item.value);
          } else if (extension.startupReason === "ADDON_ENABLE") {
            this.processDefaultSearchSetting("enable");
          }
          // If we would have set the default engine,
          // we don't allow a search provider to be added.
          return;
        }
        Components.utils.reportError("is_default can only be used for built-in engines.");
      }
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
        let params = {
          template: searchProvider.search_url,
          iconURL: searchProvider.favicon_url,
          alias: searchProvider.keyword,
          extensionID: extension.id,
          suggestURL: searchProvider.suggest_url,
        };
        Services.search.addEngineWithDetails(searchProvider.name.trim(), params);
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
    // If the setting exists for the extension, but is missing from the manifest,
    // remove it. This can happen if the extension removes is_default.
    // There's really no good place to put this, because the entire search section
    // could be removed.
    // We'll never get here in the normal case because we always return early
    // if we have an is_default value that we use.
    if (ExtensionSettingsStore.hasSetting(
               extension, DEFAULT_SEARCH_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME)) {
      await searchInitialized();
      this.processDefaultSearchSetting("removeSetting");
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
