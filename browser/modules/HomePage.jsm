/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["HomePage"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  CustomizableUI: "resource:///modules/CustomizableUI.jsm",
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
  ExtensionPreferencesManager:
    "resource://gre/modules/ExtensionPreferencesManager.jsm",
  IgnoreLists: "resource://gre/modules/IgnoreLists.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const kPrefName = "browser.startup.homepage";
const kDefaultHomePage = "about:home";
const kExtensionControllerPref =
  "browser.startup.homepage_override.extensionControlled";
const kHomePageIgnoreListId = "homepage-urls";
const kWidgetId = "home-button";
const kWidgetRemovedPref = "browser.engagement.home-button.has-removed";
const kProtonToolbarEnabledPref = "browser.proton.enabled";

function getHomepagePref(useDefault) {
  let homePage;
  let prefs = Services.prefs;
  if (useDefault) {
    prefs = prefs.getDefaultBranch(null);
  }
  try {
    // Historically, this was a localizable pref, but default Firefox builds
    // don't use this.
    // Distributions and local customizations might still use this, so let's
    // keep it.
    homePage = prefs.getComplexValue(kPrefName, Ci.nsIPrefLocalizedString).data;
  } catch (ex) {}

  if (!homePage) {
    homePage = prefs.getStringPref(kPrefName);
  }

  // Apparently at some point users ended up with blank home pages somehow.
  // If that happens, reset the pref and read it again.
  if (!homePage && !useDefault) {
    Services.prefs.clearUserPref(kPrefName);
    homePage = getHomepagePref(true);
  }

  return homePage;
}

/**
 * HomePage provides tools to keep try of the current homepage, and the
 * applications's default homepage. It includes tools to insure that certain
 * urls are ignored. As a result, all set/get requests for the homepage
 * preferences should be routed through here.
 */
let HomePage = {
  // This is an array of strings that should be matched against URLs to see
  // if they should be ignored or not.
  _ignoreList: [],

  // A promise that is set when initialization starts and resolved when it
  // completes.
  _initializationPromise: null,

  /**
   * Used to initialise the ignore lists. This may be called later than
   * the first call to get or set, which may cause a used to get an ignored
   * homepage, but this is deemed acceptable, as we'll correct it once
   * initialised.
   */
  async delayedStartup() {
    if (this._initializationPromise) {
      await this._initializationPromise;
      return;
    }

    Services.telemetry.setEventRecordingEnabled("homepage", true);

    // Now we have the values, listen for future updates.
    this._ignoreListListener = this._handleIgnoreListUpdated.bind(this);

    this._initializationPromise = IgnoreLists.getAndSubscribe(
      this._ignoreListListener
    );

    this._addCustomizableUiListener();

    const current = await this._initializationPromise;

    await this._handleIgnoreListUpdated({ data: { current } });
  },

  /**
   * Gets the homepage for the given window.
   *
   * @param {DOMWindow} [aWindow]
   *   The window associated with the get, used to check for private browsing
   *   mode. If not supplied, normal mode is assumed.
   * @returns {string}
   *   Returns the home page value, this could be a single url, or a `|`
   *   separated list of URLs.
   */
  get(aWindow) {
    let homePages = getHomepagePref();
    if (
      PrivateBrowsingUtils.permanentPrivateBrowsing ||
      (aWindow && PrivateBrowsingUtils.isWindowPrivate(aWindow))
    ) {
      // If an extension controls the setting and does not have private
      // browsing permission, use the default setting.
      let extensionControlled = Services.prefs.getBoolPref(
        kExtensionControllerPref,
        false
      );
      let privateAllowed = Services.prefs.getBoolPref(
        "browser.startup.homepage_override.privateAllowed",
        false
      );
      // There is a potential on upgrade that the prefs are not set yet, so we double check
      // for moz-extension.
      if (
        !privateAllowed &&
        (extensionControlled || homePages.includes("moz-extension://"))
      ) {
        return this.getDefault();
      }
    }

    return homePages;
  },

  /**
   * @returns {string}
   *   Returns the application default homepage.
   */
  getDefault() {
    return getHomepagePref(true);
  },

  /**
   * @returns {string}
   *   Returns the original application homepage URL (not from prefs).
   */
  getOriginalDefault() {
    return kDefaultHomePage;
  },

  /**
   * @returns {boolean}
   *   Returns true if the homepage has been changed.
   */
  get overridden() {
    return Services.prefs.prefHasUserValue(kPrefName);
  },

  /**
   * @returns {boolean}
   *   Returns true if the homepage preference is locked.
   */
  get locked() {
    return Services.prefs.prefIsLocked(kPrefName);
  },

  /**
   * @returns {boolean}
   *   Returns true if the current homepage is the application default.
   */
  get isDefault() {
    return HomePage.get() === kDefaultHomePage;
  },

  /**
   * Sets the homepage preference to a new page.
   *
   * @param {string} value
   *   The new value to set the preference to. This could be a single url, or a
   *   `|` separated list of URLs.
   */
  async set(value) {
    await this.delayedStartup();

    if (await this.shouldIgnore(value)) {
      Cu.reportError(
        `Ignoring homepage setting for ${value} as it is on the ignore list.`
      );
      Services.telemetry.recordEvent(
        "homepage",
        "preference",
        "ignore",
        "set_blocked"
      );
      return false;
    }
    Services.prefs.setStringPref(kPrefName, value);
    this._maybeAddHomeButtonToToolbar(value);
    return true;
  },

  /**
   * Sets the homepage preference to a new page. This is an synchronous version
   * that should only be used when we know the source is safe as it bypasses the
   * ignore list, e.g. when setting directly to about:blank or a value not
   * supplied externally.
   *
   * @param {string} value
   *   The new value to set the preference to. This could be a single url, or a
   *   `|` separated list of URLs.
   */
  safeSet(value) {
    Services.prefs.setStringPref(kPrefName, value);
  },

  /**
   * Clears the homepage preference if it is not the default. Note that for
   * policy/locking use, the default homepage might not be about:home after this.
   */
  clear() {
    Services.prefs.clearUserPref(kPrefName);
  },

  /**
   * Resets the homepage preference to be about:home.
   */
  reset() {
    Services.prefs.setStringPref(kPrefName, kDefaultHomePage);
  },

  /**
   * Determines if a url should be ignored according to the ignore list.
   *
   * @param {string} url
   *   A string that is the url or urls to be ignored.
   * @returns {boolean}
   *   True if the url should be ignored.
   */
  async shouldIgnore(url) {
    await this.delayedStartup();

    const lowerURL = url.toLowerCase();
    return this._ignoreList.some(code => lowerURL.includes(code.toLowerCase()));
  },

  /**
   * Handles updates of the ignore list, checking the existing preference and
   * correcting it as necessary.
   *
   * @param {Object} eventData
   *   The event data as received from RemoteSettings.
   */
  async _handleIgnoreListUpdated({ data: { current } }) {
    for (const entry of current) {
      if (entry.id == kHomePageIgnoreListId) {
        this._ignoreList = [...entry.matches];
      }
    }

    // Only check if we're overridden as we assume the default value is fine,
    // or won't be changeable (e.g. enterprise policy).
    if (this.overridden) {
      let homePages = getHomepagePref().toLowerCase();
      if (
        this._ignoreList.some(code => homePages.includes(code.toLowerCase()))
      ) {
        if (Services.prefs.getBoolPref(kExtensionControllerPref, false)) {
          if (Services.appinfo.inSafeMode) {
            // Add-ons don't get started in safe mode, so just abort this.
            // We'll get to remove them when we next start in normal mode.
            return;
          }
          // getSetting does not need the module to be loaded.
          const item = await ExtensionPreferencesManager.getSetting(
            "homepage_override"
          );
          if (item && item.id) {
            // During startup some modules may not be loaded yet, so we load
            // the setting we need prior to removal.
            await ExtensionParent.apiManager.asyncLoadModule(
              "chrome_settings_overrides"
            );
            ExtensionPreferencesManager.removeSetting(
              item.id,
              "homepage_override"
            ).catch(Cu.reportError);
          } else {
            // If we don't have a setting for it, we assume the pref has
            // been incorrectly set somehow.
            Services.prefs.clearUserPref(kExtensionControllerPref);
            Services.prefs.clearUserPref(
              "browser.startup.homepage_override.privateAllowed"
            );
          }
        } else {
          this.clear();
        }
        Services.telemetry.recordEvent(
          "homepage",
          "preference",
          "ignore",
          "saved_reset"
        );
      }
    }
  },

  onWidgetRemoved(widgetId, area) {
    if (widgetId == kWidgetId) {
      Services.prefs.setBoolPref(kWidgetRemovedPref, true);
      CustomizableUI.removeListener(this);
    }
  },

  /**
   * Add the home button to the toolbar if the user just set a custom homepage.
   *
   * This should only be done once, so we check HOME_BUTTON_REMOVED_PREF which
   * gets set to true when the home button is removed from the toolbar.
   *
   * If the home button is already on the toolbar it won't be moved.
   */
  _maybeAddHomeButtonToToolbar(homePage) {
    if (
      homePage !== "about:home" &&
      homePage !== "about:blank" &&
      Services.prefs.getBoolPref(kProtonToolbarEnabledPref, false) &&
      !Services.prefs.getBoolPref(kExtensionControllerPref, false) &&
      !Services.prefs.getBoolPref(kWidgetRemovedPref, false) &&
      !CustomizableUI.getWidget(kWidgetId).areaType
    ) {
      // Find a spot for the home button, ideally it will be in its default
      // position beside the stop/refresh button.
      // Work backwards from the URL bar since it can't be removed and put
      // the button after the first non-spring we find.
      let navbarPlacements = CustomizableUI.getWidgetIdsInArea("nav-bar");
      let position = navbarPlacements.indexOf("urlbar-container");
      for (let i = position - 1; i >= 0; i--) {
        if (!navbarPlacements[i].startsWith("customizableui-special-spring")) {
          position = i + 1;
          break;
        }
      }
      CustomizableUI.addWidgetToArea(kWidgetId, "nav-bar", position);
    }
  },

  _addCustomizableUiListener() {
    if (
      Services.prefs.getBoolPref(kProtonToolbarEnabledPref, false) &&
      !Services.prefs.getBoolPref(kWidgetRemovedPref, false)
    ) {
      CustomizableUI.addListener(this);
    }
  },
};
