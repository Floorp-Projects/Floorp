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
    preferencesLocation: "home-homeOverride",
    preferencesEntrypoint: "addon-manage-home-override",
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
  // For new installs and enabling a disabled addon, we will show
  // the prompt.  We clear the confirmation in onDisabled and
  // onUninstalled, so in either ADDON_INSTALL or ADDON_ENABLE it
  // is already cleared, resulting in the prompt being shown if
  // necessary the next time the homepage is shown.

  // For localizing the homepageUrl, or otherwise updating the value
  // we need to always set the setting here.
  let inControl = await ExtensionPreferencesManager.setSetting(
    extension.id,
    "homepage_override",
    homepageUrl
  );

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
      DEFAULT_SEARCH_SETTING_NAME,
      id
    );
    if (!item) {
      return;
    }
    let control = await ExtensionSettingsStore.getLevelOfControl(
      id,
      DEFAULT_SEARCH_STORE_TYPE,
      DEFAULT_SEARCH_SETTING_NAME
    );
    item = ExtensionSettingsStore[action](
      id,
      DEFAULT_SEARCH_STORE_TYPE,
      DEFAULT_SEARCH_SETTING_NAME
    );
    if (item && control == "controlled_by_this_extension") {
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

    try {
      await Services.search.removeWebExtensionEngine(id);
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

  static async onUninstall(id) {
    let searchStartupPromise = pendingSearchSetupTasks.get(id);
    if (searchStartupPromise) {
      await searchStartupPromise.catch(Cu.reportError);
    }
    // Note: We do not have to manage the homepage setting here
    // as it is managed by the ExtensionPreferencesManager.
    return Promise.all([
      this.removeSearchSettings(id),
      homepagePopup.clearConfirmation(id),
    ]);
  }

  static async onUpdate(id, manifest) {
    if (!manifest?.chrome_settings_overrides?.homepage) {
      // New or changed values are handled during onManifest.
      ExtensionPreferencesManager.removeSetting(id, "homepage_override");
    }

    let search_provider = manifest?.chrome_settings_overrides?.search_provider;

    if (!search_provider) {
      // Remove setting and engine from search if necessary.
      this.removeSearchSettings(id);
    } else if (!search_provider.is_default) {
      // Remove the setting, but keep the engine in search.
      chrome_settings_overrides.processDefaultSearchSetting(
        "removeSetting",
        id
      );
    }
  }

  static async onDisable(id) {
    homepagePopup.clearConfirmation(id);

    await chrome_settings_overrides.processDefaultSearchSetting("disable", id);
    await chrome_settings_overrides.removeEngine(id);
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
            // This is primarily for tests so that we know when an extension
            // has finished initialising.
            ExtensionParent.apiManager.emit("searchEngineProcessed", extension);
          }
        }
      );

      // Save the promise so we can await at onUninstall.
      pendingSearchSetupTasks.set(extension.id, searchStartupPromise);
    }
  }

  async ensureSetting(engineName, disable = false) {
    let { extension } = this;
    // Ensure the addon always has a setting
    await ExtensionSettingsStore.initialize();
    let item = ExtensionSettingsStore.getSetting(
      DEFAULT_SEARCH_STORE_TYPE,
      DEFAULT_SEARCH_SETTING_NAME,
      extension.id
    );
    if (!item) {
      let defaultEngine = await Services.search.getDefault();
      item = await ExtensionSettingsStore.addSetting(
        extension.id,
        DEFAULT_SEARCH_STORE_TYPE,
        DEFAULT_SEARCH_SETTING_NAME,
        engineName,
        () => defaultEngine.name
      );
      // If there was no setting, we're fixing old behavior in this api.
      // A lack of a setting would mean it was disabled before, disable it now.
      disable =
        disable ||
        ["ADDON_UPGRADE", "ADDON_DOWNGRADE", "ADDON_ENABLE"].includes(
          extension.startupReason
        );
    }
    if (disable && item?.enabled) {
      item = await ExtensionSettingsStore.disable(
        extension.id,
        DEFAULT_SEARCH_STORE_TYPE,
        DEFAULT_SEARCH_SETTING_NAME
      );
    }
    return item;
  }

  async promptDefaultSearch(engineName) {
    let { extension } = this;
    // Don't ask if it is already the current engine
    let engine = Services.search.getEngineByName(engineName);
    let defaultEngine = await Services.search.getDefault();
    if (defaultEngine.name == engine.name) {
      return;
    }
    // Ensures the setting exists and is disabled.  If the
    // user somehow bypasses the prompt, we do not want this
    // setting enabled for this extension.
    await this.ensureSetting(engineName, true);

    let subject = {
      wrappedJSObject: {
        // This is a hack because we don't have the browser of
        // the actual install. This means the popup might show
        // in a different window. Will be addressed in a followup bug.
        browser: windowTracker.topWindow.gBrowser.selectedBrowser,
        name: extension.name,
        icon: extension.iconURL,
        currentEngine: defaultEngine.name,
        newEngine: engineName,
        async respond(allow) {
          if (allow) {
            await chrome_settings_overrides.processDefaultSearchSetting(
              "enable",
              extension.id
            );
            Services.search.defaultEngine = Services.search.getEngineByName(
              engineName
            );
          }
          // For testing
          Services.obs.notifyObservers(
            null,
            "webextension-defaultsearch-prompt-response"
          );
        },
      },
    };
    Services.obs.notifyObservers(subject, "webextension-defaultsearch-prompt");
  }

  async processSearchProviderManifestEntry() {
    let { extension } = this;
    let { manifest } = extension;
    let searchProvider = manifest.chrome_settings_overrides.search_provider;

    // If we're not being requested to be set as default, then all we need
    // to do is to add the engine to the service. The search service can cope
    // with receiving added engines before it is initialised, so we don't have
    // to wait for it.  Search Service will also prevent overriding a builtin
    // engine appropriately.
    if (!searchProvider.is_default) {
      await this.addSearchEngine();
      return;
    }

    await searchInitialized;
    if (!this.extension) {
      Cu.reportError(
        `Extension shut down before search provider was registered`
      );
      return;
    }

    let engineName = searchProvider.name.trim();
    let result = await Services.search.maybeSetAndOverrideDefault(extension);
    if (result.canChangeToAppProvided) {
      await this.setDefault(engineName);
    }
    if (!result.canInstallEngine) {
      // This extension is overriding an app-provided one, so we don't
      // add its engine as well.
      return;
    }
    await this.addSearchEngine();
    if (extension.startupReason === "ADDON_INSTALL") {
      await this.promptDefaultSearch(engineName);
    } else {
      // Needs to be called every time to handle reenabling.
      await this.setDefault(engineName);
    }
  }

  async setDefault(engineName) {
    let { extension } = this;

    if (extension.startupReason === "ADDON_INSTALL") {
      // We should only get here if an extension is setting an app-provided
      // engine to default and we are ignoring the addons other engine settings.
      // In this case we do not show the prompt to the user.
      let item = await this.ensureSetting(engineName);
      await Services.search.setDefault(
        Services.search.getEngineByName(item.value)
      );
    } else if (
      ["ADDON_UPGRADE", "ADDON_DOWNGRADE", "ADDON_ENABLE"].includes(
        extension.startupReason
      )
    ) {
      // We would be called for every extension being enabled, we should verify
      // that it has control and only then set it as default
      let control = await ExtensionSettingsStore.getLevelOfControl(
        extension.id,
        DEFAULT_SEARCH_STORE_TYPE,
        DEFAULT_SEARCH_SETTING_NAME
      );
      if (control === "controlled_by_this_extension") {
        await Services.search.setDefault(
          Services.search.getEngineByName(engineName)
        );
      } else if (control === "controllable_by_this_extension") {
        // This extension has precedence, but is not in control.  Ask the user.
        await this.promptDefaultSearch(engineName);
      }
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
