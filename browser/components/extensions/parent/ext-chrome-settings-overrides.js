/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(this, "ExtensionPreferencesManager",
                               "resource://gre/modules/ExtensionPreferencesManager.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionSettingsStore",
                               "resource://gre/modules/ExtensionSettingsStore.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionControlledPopup",
                               "resource:///modules/ExtensionControlledPopup.jsm");

const DEFAULT_SEARCH_STORE_TYPE = "default_search";
const DEFAULT_SEARCH_SETTING_NAME = "defaultSearch";
const ENGINE_ADDED_SETTING_NAME = "engineAdded";

const HOMEPAGE_PREF = "browser.startup.homepage";
const HOMEPAGE_CONFIRMED_TYPE = "homepageNotification";
const HOMEPAGE_SETTING_TYPE = "prefs";
const HOMEPAGE_SETTING_NAME = "homepage_override";

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

XPCOMUtils.defineLazyGetter(this, "homepagePopup", () => {
  return new ExtensionControlledPopup({
    confirmedType: HOMEPAGE_CONFIRMED_TYPE,
    observerTopic: "browser-open-homepage-start",
    popupnotificationId: "extension-homepage-notification",
    settingType: HOMEPAGE_SETTING_TYPE,
    settingKey: HOMEPAGE_SETTING_NAME,
    descriptionId: "extension-homepage-notification-description",
    descriptionMessageId: "homepageControlled.message",
    learnMoreMessageId: "homepageControlled.learnMore",
    learnMoreLink: "extension-home",
    async beforeDisableAddon(popup, win) {
      // Disabling an add-on should remove the tabs that it has open, but we want
      // to open the new homepage in this tab (which might get closed).
      //   1. Replace the tab's URL with about:blank, wait for it to change
      //   2. Now that this tab isn't associated with the add-on, disable the add-on
      //   3. Trigger the browser's homepage method
      let gBrowser = win.gBrowser;
      let tab = gBrowser.selectedTab;
      await replaceUrlInTab(gBrowser, tab, "about:blank");
      Services.prefs.addObserver(HOMEPAGE_PREF, async function prefObserver() {
        Services.prefs.removeObserver(HOMEPAGE_PREF, prefObserver);
        let loaded = waitForTabLoaded(tab);
        win.BrowserGoHome();
        await loaded;
        // Manually trigger an event in case this is controlled again.
        popup.open();
      });
    },
  });
});

// When the browser starts up it will trigger the observer topic we're expecting
// but that happens before our observer has been registered. To handle the
// startup case we need to check if the preferences are set to load the homepage
// and check if the homepage is active, then show the doorhanger in that case.
async function handleInitialHomepagePopup(extensionId, homepageUrl) {
  // browser.startup.page == 1 is show homepage.
  if (Services.prefs.getIntPref("browser.startup.page") == 1) {
    let {gBrowser} = windowTracker.topWindow;
    let tab = gBrowser.selectedTab;
    let currentUrl = gBrowser.currentURI.spec;
    // When the first window is still loading the URL might be about:blank.
    // Wait for that the actual page to load before checking the URL, unless
    // the homepage is set to about:blank.
    if (currentUrl != homepageUrl && currentUrl == "about:blank") {
      await waitForTabLoaded(tab);
      currentUrl = gBrowser.currentURI.spec;
    }
    // Once the page has loaded, if necessary and the active tab hasn't changed,
    // then show the popup now.
    if (currentUrl == homepageUrl && gBrowser.selectedTab == tab) {
      homepagePopup.open();
      return;
    }
  }
  homepagePopup.addObserver(extensionId);
}

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
        Cu.reportError(e);
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
        Cu.reportError(e);
      }
    }
  }

  static removeSearchSettings(id) {
    return Promise.all([
      this.processDefaultSearchSetting("removeSetting", id),
      this.removeEngine(id),
    ]);
  }

  static onUninstall(id) {
    // Note: We do not have to deal with homepage here as it is managed by
    // the ExtensionPreferencesManager.
    return Promise.all([
      this.removeSearchSettings(id),
      homepagePopup.clearConfirmation(id),
    ]);
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

    let homepageUrl = manifest.chrome_settings_overrides.homepage;

    if (homepageUrl) {
      let inControl;
      if (extension.startupReason == "ADDON_INSTALL") {
        inControl = await ExtensionPreferencesManager.setSetting(
          extension.id, "homepage_override", homepageUrl);
      } else {
        let item = await ExtensionPreferencesManager.getSetting("homepage_override");
        inControl = item.id == extension.id;
      }
      // We need to add the listener here too since onPrefsChanged won't trigger on a
      // restart (the prefs are already set).
      if (inControl) {
        if (extension.startupReason == "APP_STARTUP") {
          handleInitialHomepagePopup(extension.id, homepageUrl);
        } else {
          homepagePopup.addObserver(extension.id);
        }
      }
      extension.callOnClose({
        close: () => {
          if (extension.shutdownReason == "ADDON_DISABLE") {
            homepagePopup.clearConfirmation(extension.id);
          }
        },
      });
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
      Cu.reportError(e);
      return false;
    }
    return true;
  }
};

ExtensionPreferencesManager.addSetting("homepage_override", {
  prefNames: [
    HOMEPAGE_PREF,
  ],
  // ExtensionPreferencesManager will call onPrefsChanged when control changes
  // and it updates the preferences. We are passed the item from
  // ExtensionSettingsStore that details what is in control. If there is an id
  // then control has changed to an extension, if there is no id then control
  // has been returned to the user.
  onPrefsChanged(item) {
    if (item.id) {
      homepagePopup.addObserver(item.id);
    } else {
      homepagePopup.removeObserver();
    }
  },
  setCallback(value) {
    return {
      [HOMEPAGE_PREF]: value,
    };
  },
});
