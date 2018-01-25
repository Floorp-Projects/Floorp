/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals windowTracker */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionPreferencesManager",
                                  "resource://gre/modules/ExtensionPreferencesManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");

const DEFAULT_SEARCH_STORE_TYPE = "default_search";
const DEFAULT_SEARCH_SETTING_NAME = "defaultSearch";
const ENGINE_ADDED_SETTING_NAME = "engineAdded";

// This promise is used to wait for the search service to be initialized.
// None of the code in this module requests that initialization. It is assumed
// that it is started at some point. If tests start to fail because this
// promise never resolves, that's likely the cause.
const searchInitialized = () => {
  if (Services.search.isInitialized) {
    return;
  }
  return new Promise(resolve => {
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
  static async processDefaultSearchSetting(action, id) {
    await ExtensionSettingsStore.initialize();
    let item = ExtensionSettingsStore.getSetting(DEFAULT_SEARCH_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME);
    if (!item) {
      return;
    }
    if (Services.search.currentEngine.name != item.value &&
        Services.search.currentEngine.name != item.initialValue) {
      // The current engine is not the same as the value that the ExtensionSettingsStore has.
      // This means that the user changed the engine, so we shouldn't control it anymore.
      // Do nothing and remove our entry from the ExtensionSettingsStore.
      ExtensionSettingsStore.removeSetting(id, DEFAULT_SEARCH_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME);
      return;
    }
    item = ExtensionSettingsStore[action](id, DEFAULT_SEARCH_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME);
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

  static async removeEngine(id) {
    await ExtensionSettingsStore.initialize();
    let item = await ExtensionSettingsStore.getSetting(
      DEFAULT_SEARCH_STORE_TYPE, ENGINE_ADDED_SETTING_NAME, id);
    if (item) {
      ExtensionSettingsStore.removeSetting(
        id, DEFAULT_SEARCH_STORE_TYPE, ENGINE_ADDED_SETTING_NAME);
      await searchInitialized();
      let engine = Services.search.getEngineByName(item.value);
      try {
        Services.search.removeEngine(engine);
      } catch (e) {
        Components.utils.reportError(e);
      }
    }
  }

  static removeSearchSettings(id) {
    this.processDefaultSearchSetting("removeSetting", id);
    this.removeEngine(id);
  }

  static onUninstall(id) {
    // Note: We do not have to deal with homepage here as it is managed by
    // the ExtensionPreferencesManager.
    this.removeSearchSettings(id);
  }

  static onUpdate(id, manifest) {
    let haveHomepage = manifest && manifest.chrome_settings_overrides &&
                       manifest.chrome_settings_overrides.homepage;
    if (!haveHomepage) {
      ExtensionPreferencesManager.removeSetting(id, "homepage_override");
    }

    let haveSearchProvider = manifest && manifest.chrome_settings_overrides &&
                             manifest.chrome_settings_overrides.search_provider;
    if (!haveSearchProvider) {
      this.removeSearchSettings(id);
    }
  }

  async onManifestEntry(entryName) {
    let {extension} = this;
    let {manifest} = extension;

    await ExtensionSettingsStore.initialize();
    if (manifest.chrome_settings_overrides.homepage) {
      ExtensionPreferencesManager.setSetting(extension.id, "homepage_override",
                                             manifest.chrome_settings_overrides.homepage);
    }
    if (manifest.chrome_settings_overrides.search_provider) {
      await searchInitialized();
      extension.callOnClose({
        close: () => {
          if (extension.shutdownReason == "ADDON_DISABLE") {
            chrome_settings_overrides.processDefaultSearchSetting("disable", extension.id);
            chrome_settings_overrides.removeEngine(extension.id);
          }
        },
      });

      let searchProvider = manifest.chrome_settings_overrides.search_provider;
      let engineName = searchProvider.name.trim();
      if (searchProvider.is_default) {
        let engine = Services.search.getEngineByName(engineName);
        if (engine && Services.search.getDefaultEngines().includes(engine)) {
          // Needs to be called every time to handle reenabling, but
          // only sets default for install or enable.
          await this.setDefault(engineName);
          // For built in search engines, we don't do anything further
          return;
        }
      }
      await this.addSearchEngine(searchProvider);
      if (searchProvider.is_default) {
        if (extension.startupReason === "ADDON_INSTALL") {
          // Don't ask if it already the current engine
          let engine = Services.search.getEngineByName(engineName);
          if (Services.search.currentEngine != engine) {
            let allow = await new Promise(resolve => {
              let subject = {
                wrappedJSObject: {
                  // This is a hack because we don't have the browser of
                  // the actual install. This means the popup might show
                  // in a different window. Will be addressed in a followup bug.
                  browser: windowTracker.topWindow.gBrowser.selectedBrowser,
                  name: this.extension.name,
                  icon: this.extension.iconURL,
                  currentEngine: Services.search.currentEngine.name,
                  newEngine: engineName,
                  resolve,
                },
              };
              Services.obs.notifyObservers(subject, "webextension-defaultsearch-prompt");
            });
            if (!allow) {
              return;
            }
          }
        }
        // Needs to be called every time to handle reenabling, but
        // only sets default for install or enable.
        await this.setDefault(engineName);
      } else if (ExtensionSettingsStore.hasSetting(extension.id,
                                                   DEFAULT_SEARCH_STORE_TYPE,
                                                   DEFAULT_SEARCH_SETTING_NAME)) {
        // is_default has been removed, but we still have a setting. Remove it.
        chrome_settings_overrides.processDefaultSearchSetting("removeSetting", extension.id);
      }
    }
  }

  async setDefault(engineName) {
    let {extension} = this;
    if (extension.startupReason === "ADDON_INSTALL") {
      let item = await ExtensionSettingsStore.addSetting(
        extension.id, DEFAULT_SEARCH_STORE_TYPE, DEFAULT_SEARCH_SETTING_NAME, engineName, () => {
          return Services.search.currentEngine.name;
        });
      Services.search.currentEngine = Services.search.getEngineByName(item.value);
    } else if (extension.startupReason === "ADDON_ENABLE") {
      chrome_settings_overrides.processDefaultSearchSetting("enable", extension.id);
    }
  }

  async addSearchEngine(searchProvider) {
    let {extension} = this;
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
        queryCharset: "UTF-8",
      };
      Services.search.addEngineWithDetails(searchProvider.name.trim(), params);
      await ExtensionSettingsStore.addSetting(
        extension.id, DEFAULT_SEARCH_STORE_TYPE, ENGINE_ADDED_SETTING_NAME,
        searchProvider.name.trim());
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
      return false;
    }
    return true;
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
