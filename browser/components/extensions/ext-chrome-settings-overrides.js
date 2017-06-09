/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionPreferencesManager",
                                  "resource://gre/modules/ExtensionPreferencesManager.jsm");

function searchInitialized() {
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
}

this.chrome_settings_overrides = class extends ExtensionAPI {
  async onManifestEntry(entryName) {
    let {extension} = this;
    let {manifest} = extension;

    if (manifest.chrome_settings_overrides.homepage) {
      ExtensionPreferencesManager.setSetting(extension, "homepage_override",
                                             manifest.chrome_settings_overrides.homepage);
    }
    if (manifest.chrome_settings_overrides.search_provider) {
      await searchInitialized();
      let searchProvider = manifest.chrome_settings_overrides.search_provider;
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
