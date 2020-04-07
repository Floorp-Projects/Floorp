/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionPreferencesManager } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPreferencesManager.jsm"
);
var { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionPermissions",
  "resource://gre/modules/ExtensionPermissions.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionSettingsStore",
  "resource://gre/modules/ExtensionSettingsStore.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionControlledPopup",
  "resource:///modules/ExtensionControlledPopup.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "HomePage",
  "resource:///modules/HomePage.jsm"
);

const DEFAULT_SEARCH_STORE_TYPE = "default_search";
const DEFAULT_SEARCH_SETTING_NAME = "defaultSearch";
const ENGINE_ADDED_SETTING_NAME = "engineAdded";

const HOMEPAGE_PREF = "browser.startup.homepage";
const HOMEPAGE_PRIVATE_ALLOWED =
  "browser.startup.homepage_override.privateAllowed";
const HOMEPAGE_EXTENSION_CONTROLLED =
  "browser.startup.homepage_override.extensionControlled";
const HOMEPAGE_CONFIRMED_TYPE = "homepageNotification";
const HOMEPAGE_SETTING_TYPE = "prefs";
const HOMEPAGE_SETTING_NAME = "homepage_override";

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
        win.BrowserHome();
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
  if (
    Services.prefs.getIntPref("browser.startup.page") == 1 &&
    windowTracker.topWindow
  ) {
    let { gBrowser } = windowTracker.topWindow;
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

/**
 * Handles the homepage url setting for an extension.
 *
 * @param {object} extension
 *   The extension setting the hompage url.
 * @param {string} homepageUrl
 *   The homepage url to set.
 */
async function handleHomepageUrl(extension, homepageUrl) {
  let inControl;
  if (
    extension.startupReason == "ADDON_INSTALL" ||
    extension.startupReason == "ADDON_ENABLE"
  ) {
    inControl = await ExtensionPreferencesManager.setSetting(
      extension.id,
      "homepage_override",
      homepageUrl
    );
  } else {
    let item = await ExtensionPreferencesManager.getSetting(
      "homepage_override"
    );
    inControl = item && item.id && item.id == extension.id;
  }

  if (inControl) {
    Services.prefs.setBoolPref(
      HOMEPAGE_PRIVATE_ALLOWED,
      extension.privateBrowsingAllowed
    );
    // Also set this now as an upgraded browser will need this.
    Services.prefs.setBoolPref(HOMEPAGE_EXTENSION_CONTROLLED, true);
    if (extension.startupReason == "APP_STARTUP") {
      handleInitialHomepagePopup(extension.id, homepageUrl);
    } else {
      homepagePopup.addObserver(extension.id);
    }
  }

  // We need to monitor permission change and update the preferences.
  // eslint-disable-next-line mozilla/balanced-listeners
  extension.on("add-permissions", async (ignoreEvent, permissions) => {
    if (permissions.permissions.includes("internal:privateBrowsingAllowed")) {
      let item = await ExtensionPreferencesManager.getSetting(
        "homepage_override"
      );
      if (item && item.id == extension.id) {
        Services.prefs.setBoolPref(HOMEPAGE_PRIVATE_ALLOWED, true);
      }
    }
  });
  // eslint-disable-next-line mozilla/balanced-listeners
  extension.on("remove-permissions", async (ignoreEvent, permissions) => {
    if (permissions.permissions.includes("internal:privateBrowsingAllowed")) {
      let item = await ExtensionPreferencesManager.getSetting(
        "homepage_override"
      );
      if (item && item.id == extension.id) {
        Services.prefs.setBoolPref(HOMEPAGE_PRIVATE_ALLOWED, false);
      }
    }
  });
}

// When an extension starts up, a search engine may asynchronously be
// registered, without blocking the startup. When an extension is
// uninstalled, we need to wait for this registration to finish
// before running the uninstallation handler.
// Map[extension id -> Promise]
var pendingSearchSetupTasks = new Map();

this.chrome_settings_overrides = class extends ExtensionAPI {
  static async processDefaultSearchSetting(action, id) {
    await ExtensionSettingsStore.initialize();
    let item = ExtensionSettingsStore.getSetting(
      DEFAULT_SEARCH_STORE_TYPE,
      DEFAULT_SEARCH_SETTING_NAME
    );
    if (!item) {
      return;
    }
    if (
      Services.search.defaultEngine.name != item.value &&
      Services.search.defaultEngine.name != item.initialValue
    ) {
      // The current engine is not the same as the value that the ExtensionSettingsStore has.
      // This means that the user changed the engine, so we shouldn't control it anymore.
      // Do nothing and remove our entry from the ExtensionSettingsStore.
      ExtensionSettingsStore.removeSetting(
        id,
        DEFAULT_SEARCH_STORE_TYPE,
        DEFAULT_SEARCH_SETTING_NAME
      );
      return;
    }
    item = ExtensionSettingsStore[action](
      id,
      DEFAULT_SEARCH_STORE_TYPE,
      DEFAULT_SEARCH_SETTING_NAME
    );
    if (item) {
      try {
        let engine = Services.search.getEngineByName(
          item.value || item.initialValue
        );
        if (engine) {
          Services.search.defaultEngine = engine;
        }
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }

  static async removeEngine(id) {
    await ExtensionSettingsStore.initialize();
    let item = await ExtensionSettingsStore.getSetting(
      DEFAULT_SEARCH_STORE_TYPE,
      ENGINE_ADDED_SETTING_NAME,
      id
    );
    if (item) {
      ExtensionSettingsStore.removeSetting(
        id,
        DEFAULT_SEARCH_STORE_TYPE,
        ENGINE_ADDED_SETTING_NAME
      );
    }
    // We can call removeEngine in nsSearchService startup, if so we dont
    // need to reforward the call, just disable the web extension.
    if (!Services.search.isInitialized) {
      return;
    }

    try {
      let engines = await Services.search.getEnginesByExtensionID(id);
      if (engines.length) {
        await Services.search.removeWebExtensionEngine(id);
      }
    } catch (e) {
      Cu.reportError(e);
    }
  }

  static removeSearchSettings(id) {
    return Promise.all([
      this.processDefaultSearchSetting("removeSetting", id),
      this.removeEngine(id),
    ]);
  }

  static async onEnabling(id) {
    chrome_settings_overrides.processDefaultSearchSetting("enable", id);
  }

  static async onUninstall(id) {
    let searchStartupPromise = pendingSearchSetupTasks.get(id);
    if (searchStartupPromise) {
      await searchStartupPromise;
    }
    // Note: We do not have to deal with homepage here as it is managed by
    // the ExtensionPreferencesManager.
    return Promise.all([
      this.removeSearchSettings(id),
      homepagePopup.clearConfirmation(id),
    ]);
  }

  static async onUpdate(id, manifest) {
    let haveHomepage =
      manifest &&
      manifest.chrome_settings_overrides &&
      manifest.chrome_settings_overrides.homepage;

    if (!haveHomepage) {
      ExtensionPreferencesManager.removeSetting(id, "homepage_override");
    }

    let haveSearchProvider =
      manifest &&
      manifest.chrome_settings_overrides &&
      manifest.chrome_settings_overrides.search_provider;

    if (!haveSearchProvider) {
      this.removeSearchSettings(id);
    } else if (
      !!haveSearchProvider.is_default &&
      (await ExtensionSettingsStore.initialize()) &&
      ExtensionSettingsStore.hasSetting(
        id,
        DEFAULT_SEARCH_STORE_TYPE,
        DEFAULT_SEARCH_SETTING_NAME
      )
    ) {
      // is_default has been removed, but we still have a setting. Remove it.
      chrome_settings_overrides.processDefaultSearchSetting(
        "removeSetting",
        id
      );
    }
  }

  static onDisable(id) {
    homepagePopup.clearConfirmation(id);

    chrome_settings_overrides.processDefaultSearchSetting("disable", id);
    chrome_settings_overrides.removeEngine(id);
  }

  async onManifestEntry(entryName) {
    let { extension } = this;
    let { manifest } = extension;
    let homepageUrl = manifest.chrome_settings_overrides.homepage;

    // If this is a page we ignore, just skip the homepage setting completely.
    if (homepageUrl) {
      const ignoreHomePageUrl = await HomePage.shouldIgnore(homepageUrl);

      if (ignoreHomePageUrl) {
        Services.telemetry.recordEvent(
          "homepage",
          "preference",
          "ignore",
          "set_blocked_extension",
          {
            webExtensionId: extension.id,
          }
        );
      } else {
        await handleHomepageUrl(extension, homepageUrl);
      }
    }
    if (manifest.chrome_settings_overrides.search_provider) {
      // Registering a search engine can potentially take a long while,
      // or not complete at all (when searchInitialized is never resolved),
      // so we are deliberately not awaiting the returned promise here.
      let searchStartupPromise = this.processSearchProviderManifestEntry().finally(
        () => {
          if (
            pendingSearchSetupTasks.get(extension.id) === searchStartupPromise
          ) {
            pendingSearchSetupTasks.delete(extension.id);
          }
        }
      );

      // Save the promise so we can await at onUninstall.
      pendingSearchSetupTasks.set(extension.id, searchStartupPromise);
    }
  }

  async processSearchProviderManifestEntry() {
    let { extension } = this;
    let { manifest } = extension;
    let searchProvider = manifest.chrome_settings_overrides.search_provider;
    if (searchProvider.is_default) {
      await searchInitialized;
      if (!this.extension) {
        Cu.reportError(
          `Extension shut down before search provider was registered`
        );
        return;
      }
    }

    let engineName = searchProvider.name.trim();
    if (searchProvider.is_default) {
      let engine = Services.search.getEngineByName(engineName);
      if (engine && engine.isAppProvided) {
        // Needs to be called every time to handle reenabling, but
        // only sets default for install or enable.
        await this.setDefault(engineName);
        // For built in search engines, we don't do anything further
        return;
      }
    }
    await this.addSearchEngine();
    if (searchProvider.is_default) {
      if (extension.startupReason === "ADDON_INSTALL") {
        // Don't ask if it already the current engine
        let engine = Services.search.getEngineByName(engineName);
        let defaultEngine = await Services.search.getDefault();
        if (defaultEngine.name != engine.name) {
          let subject = {
            wrappedJSObject: {
              // This is a hack because we don't have the browser of
              // the actual install. This means the popup might show
              // in a different window. Will be addressed in a followup bug.
              browser: windowTracker.topWindow.gBrowser.selectedBrowser,
              name: this.extension.name,
              icon: this.extension.iconURL,
              currentEngine: defaultEngine.name,
              newEngine: engineName,
              async respond(allow) {
                if (allow) {
                  await ExtensionSettingsStore.initialize();
                  ExtensionSettingsStore.addSetting(
                    extension.id,
                    DEFAULT_SEARCH_STORE_TYPE,
                    DEFAULT_SEARCH_SETTING_NAME,
                    engineName,
                    () => defaultEngine.name
                  );
                  Services.search.defaultEngine = Services.search.getEngineByName(
                    engineName
                  );
                }
              },
            },
          };
          Services.obs.notifyObservers(
            subject,
            "webextension-defaultsearch-prompt"
          );
        }
      } else {
        // Needs to be called every time to handle reenabling, but
        // only sets default for install or enable.
        this.setDefault(engineName);
      }
    }
  }

  async setDefault(engineName) {
    let { extension } = this;
    if (extension.startupReason === "ADDON_INSTALL") {
      let defaultEngine = await Services.search.getDefault();
      await ExtensionSettingsStore.initialize();
      let item = await ExtensionSettingsStore.addSetting(
        extension.id,
        DEFAULT_SEARCH_STORE_TYPE,
        DEFAULT_SEARCH_SETTING_NAME,
        engineName,
        () => defaultEngine.name
      );
      await Services.search.setDefault(
        Services.search.getEngineByName(item.value)
      );
    } else if (extension.startupReason === "ADDON_ENABLE") {
      chrome_settings_overrides.processDefaultSearchSetting(
        "enable",
        extension.id
      );
    }
  }

  async addSearchEngine() {
    let { extension } = this;
    try {
      let engines = await Services.search.addEnginesFromExtension(extension);
      if (engines.length) {
        await ExtensionSettingsStore.initialize();
        await ExtensionSettingsStore.addSetting(
          extension.id,
          DEFAULT_SEARCH_STORE_TYPE,
          ENGINE_ADDED_SETTING_NAME,
          engines[0].name
        );
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
    HOMEPAGE_EXTENSION_CONTROLLED,
    HOMEPAGE_PRIVATE_ALLOWED,
  ],
  // ExtensionPreferencesManager will call onPrefsChanged when control changes
  // and it updates the preferences. We are passed the item from
  // ExtensionSettingsStore that details what is in control. If there is an id
  // then control has changed to an extension, if there is no id then control
  // has been returned to the user.
  async onPrefsChanged(item) {
    if (item.id) {
      homepagePopup.addObserver(item.id);

      let policy = ExtensionParent.WebExtensionPolicy.getByID(item.id);
      let allowed = policy && policy.privateBrowsingAllowed;
      if (!policy) {
        // We'll generally hit this path during safe mode changes.
        let perms = await ExtensionPermissions.get(item.id);
        allowed = perms.permissions.includes("internal:privateBrowsingAllowed");
      }
      Services.prefs.setBoolPref(HOMEPAGE_PRIVATE_ALLOWED, allowed);
      Services.prefs.setBoolPref(HOMEPAGE_EXTENSION_CONTROLLED, true);
    } else {
      homepagePopup.removeObserver();

      Services.prefs.clearUserPref(HOMEPAGE_PRIVATE_ALLOWED);
      Services.prefs.clearUserPref(HOMEPAGE_EXTENSION_CONTROLLED);
    }
  },
  setCallback(value) {
    // Setting the pref will result in onPrefsChanged being called, which
    // will then set HOMEPAGE_PRIVATE_ALLOWED.  We want to ensure that this
    // pref will be set/unset as apropriate.
    return {
      [HOMEPAGE_PREF]: value,
      [HOMEPAGE_EXTENSION_CONTROLLED]: !!value,
      [HOMEPAGE_PRIVATE_ALLOWED]: false,
    };
  },
});
