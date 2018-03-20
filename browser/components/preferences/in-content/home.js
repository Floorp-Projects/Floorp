/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* import-globals-from extensionControlled.js */
 /* import-globals-from preferences.js */

 // HOME PAGE

 /*
   * Preferences:
   *
   * browser.startup.homepage
   * - the user's home page, as a string; if the home page is a set of tabs,
   *   this will be those URLs separated by the pipe character "|"
   * browser.newtabpage.enabled
   * - determines that is shown on the user's new tab page.
   *   true = Activity Stream is shown,
   *   false = about:blank is shown
   */

Preferences.addAll([
  { id: "browser.startup.homepage", type: "wstring" },
  { id: "pref.browser.homepage.disable_button.current_page", type: "bool" },
  { id: "pref.browser.homepage.disable_button.bookmark_page", type: "bool" },
  { id: "pref.browser.homepage.disable_button.restore_default", type: "bool" },
  { id: "browser.newtabpage.enabled", type: "bool" }
]);

const HOMEPAGE_OVERRIDE_KEY = "homepage_override";
const URL_OVERRIDES_TYPE = "url_overrides";
const NEW_TAB_KEY = "newTabURL";
const NEW_TAB_STRING_ID = "extensionControlled.newTabURL2";

let gHomePane = {
  HOME_MODE_FIREFOX_HOME: "0",
  HOME_MODE_BLANK: "1",
  HOME_MODE_CUSTOM: "2",
  NEWTAB_ENABLED_PREF: "browser.newtabpage.enabled",

  /**
   * _handleNewTabOverrides: disables new tab settings UI. Called by
   * an observer in ._watchNewTab that watches for new tab url changes
   */
  async _handleNewTabOverrides() {
    const isControlled = await handleControllingExtension(
      URL_OVERRIDES_TYPE, NEW_TAB_KEY, NEW_TAB_STRING_ID);
    const el = document.getElementById("newTabMode");
    el.disabled = isControlled;
  },

  /**
   * watchNewTab: Listen for changes to the new tab url and disable appropriate
   * areas of the UI
   */
  watchNewTab() {
    this._handleNewTabOverrides();
    let newTabObserver = {
      observe: this._handleNewTabOverrides.bind(this)
    };
    Services.obs.addObserver(newTabObserver, "newtab-url-changed");
    window.addEventListener("unload", () => {
      Services.obs.removeObserver(newTabObserver, "newtab-url-changed");
    });
  },

  /**
   * _renderCustomSettings: Hides or shows the UI for setting a custom
   * homepage URL
   * @param {bool} shouldShow Should the custom UI be shown?
   */
  _renderCustomSettings(shouldShow) {
    const customSettingsContainerEl = document.getElementById("customSettings");
    const customUrlEl = document.getElementById("homePageUrl");
    const isHomepageCustom = !this._isHomePageDefaultValue() && !this._isHomePageBlank();
    if (typeof shouldShow === "undefined") shouldShow = isHomepageCustom;

    customSettingsContainerEl.hidden = !shouldShow;
    if (isHomepageCustom) {
      customUrlEl.value = Preferences.get("browser.startup.homepage").value;
    } else {
      customUrlEl.value = "";
    }
  },

  /**
   * _isHomePageDefaultValue
   * @returns {bool} Is the homepage set to the default pref value?
   */
  _isHomePageDefaultValue() {
    const homePref = Preferences.get("browser.startup.homepage");
    return homePref.value === homePref.defaultValue;
  },

  /**
   * _isHomePageBlank
   * @returns {bool} Is the homepage set to about:blank?
   */
  _isHomePageBlank() {
    const homePref = Preferences.get("browser.startup.homepage");
    return homePref.value === "about:blank" || homePref.value === "";
  },

  /**
   * _isTabAboutPreferences: Is a given tab set to about:preferences?
   * @param {Element} aTab A tab element
   * @returns {bool} Is the linkedBrowser of aElement set to about:preferences?
   */
  _isTabAboutPreferences(aTab) {
    return aTab.linkedBrowser.currentURI.spec.startsWith("about:preferences");
  },

  /**
   * _getTabsForHomePage
   * @returns {Array} An array of current tabs
   */
  _getTabsForHomePage() {
    let tabs = [];
    let win = Services.wm.getMostRecentWindow("navigator:browser");

    // We should only include visible & non-pinned tabs
    if (win && win.document.documentElement.getAttribute("windowtype") === "navigator:browser") {
      tabs = win.gBrowser.visibleTabs.slice(win.gBrowser._numPinnedTabs);
      tabs = tabs.filter(tab => !this._isTabAboutPreferences(tab));
      // XXX: Bug 1441637 - Fix tabbrowser to report tab.closing before it blurs it
      tabs = tabs.filter(tab => !tab.closing);
    }

    return tabs;
  },

  _renderHomepageMode() {
    const isDefault = this._isHomePageDefaultValue();
    const isBlank = this._isHomePageBlank();
    const el = document.getElementById("homeMode");

    if (isDefault) {
      el.value = this.HOME_MODE_FIREFOX_HOME;
    } else if (isBlank) {
      el.value = this.HOME_MODE_BLANK;
    } else {
      el.value = this.HOME_MODE_CUSTOM;
    }
  },

  _setInputDisabledStates(isControlled) {
    let tabCount = this._getTabsForHomePage().length;

    // Disable or enable the inputs based on if this is controlled by an extension.
    document.querySelectorAll(".check-home-page-controlled")
      .forEach((element) => {
        let isDisabled;
        let pref = element.getAttribute("preference") || element.getAttribute("data-preference-related");
        if (!pref) {
          throw new Error(`Element with id ${element.id} did not have preference or data-preference-related attribute defined.`);
        }
        isDisabled = Preferences.get(pref).locked || isControlled;

        if (pref === "pref.browser.disable_button.current_page") {
          // Special case for current_page to disable it if tabCount is 0
          isDisabled = isDisabled || tabCount < 1;
        }

        element.disabled = isDisabled;
      });
  },

  async _handleHomePageOverrides() {
    const homePref = Preferences.get("browser.startup.homepage");
    if (homePref.locked) {
      // An extension can't control these settings if they're locked.
      hideControllingExtension(HOMEPAGE_OVERRIDE_KEY);
      this._setInputDisabledStates(false);
    } else {
      // Asynchronously update the extension controlled UI.
      const isHomePageControlled = await handleControllingExtension(
        PREF_SETTING_TYPE, HOMEPAGE_OVERRIDE_KEY, "extensionControlled.homepage_override2");
      this._setInputDisabledStates(isHomePageControlled);
    }
  },

  syncFromHomePref() {
    // Set the "Use Current Page(s)" button's text and enabled state.
    this._updateUseCurrentButton();
    this._renderCustomSettings();
    this._renderHomepageMode();
    this._handleHomePageOverrides();
  },

  syncFromNewTabPref() {
    const newtabPref = Preferences.get(this.NEWTAB_ENABLED_PREF);
    return newtabPref.value ? this.HOME_MODE_FIREFOX_HOME : this.HOME_MODE_BLANK;
  },

  syncToNewTabPref(value) {
    return value !== this.HOME_MODE_BLANK;
  },

  onMenuChange(event) {
    const {value} = event.target;
    const homePref = Preferences.get("browser.startup.homepage");
    switch (value) {
      case this.HOME_MODE_FIREFOX_HOME:
        if (homePref.value !== homePref.defaultValue) {
          homePref.value = homePref.defaultValue;
        } else {
          this._renderCustomSettings(false);
        }
        break;
      case this.HOME_MODE_BLANK:
        if (homePref.value !== "about:blank") {
          homePref.value = "about:blank";
        } else {
          this._renderCustomSettings(false);
        }
        break;
      case this.HOME_MODE_CUSTOM:
        this._renderCustomSettings(true);
        break;
    }
  },

  /**
   * Switches the "Use Current Page" button between its singular and plural
   * forms.
   */
  async _updateUseCurrentButton() {
    let useCurrent = document.getElementById("useCurrentBtn");
    let tabs = this._getTabsForHomePage();
    const tabCount = tabs.length;
    document.l10n.setAttributes(useCurrent, "use-current-pages", { tabCount });

    // If the homepage is controlled by an extension then you can't use this.
    if (await getControllingExtensionInfo(PREF_SETTING_TYPE, HOMEPAGE_OVERRIDE_KEY)) {
      return;
    }

    // In this case, the button's disabled state is set by preferences.xml.
    let prefName = "pref.browser.homepage.disable_button.current_page";
    if (Preferences.get(prefName).locked)
      return;

    useCurrent.disabled = tabCount < 1;
  },

  /**
   * Sets the home page to the URL(s) of any currently opened tab(s),
   * updating about:preferences#home UI to reflect this.
   */
  setHomePageToCurrent() {
    let homePage = Preferences.get("browser.startup.homepage");
    let tabs = this._getTabsForHomePage();
    function getTabURI(t) {
      return t.linkedBrowser.currentURI.spec;
    }

    // FIXME Bug 244192: using dangerous "|" joiner!
    if (tabs.length) {
      homePage.value = tabs.map(getTabURI).join("|");
    }

    Services.telemetry.scalarAdd("preferences.use_current_page", 1);
  },

  _setHomePageToBookmarkClosed(rv, aEvent) {
    if (aEvent.detail.button != "accept")
      return;
    if (rv.urls && rv.names) {
      let homePage = Preferences.get("browser.startup.homepage");

      // XXX still using dangerous "|" joiner!
      homePage.value = rv.urls.join("|");
    }
  },

   /**
   * Displays a dialog in which the user can select a bookmark to use as home
   * page.  If the user selects a bookmark, that bookmark's name is displayed in
   * UI and the bookmark's address is stored to the home page preference.
   */
  setHomePageToBookmark() {
    const rv = { urls: null, names: null };
    gSubDialog.open("chrome://browser/content/preferences/selectBookmark.xul",
      "resizable=yes, modal=yes", rv,
      this._setHomePageToBookmarkClosed.bind(this, rv));
    Services.telemetry.scalarAdd("preferences.use_bookmark", 1);
  },

  restoreDefaultHomePage() {
    const homePref = Preferences.get("browser.startup.homepage");
    const newtabPref = Preferences.get(this.NEWTAB_ENABLED_PREF);
    homePref.value = homePref.defaultValue;
    newtabPref.value = newtabPref.defaultValue;
  },

  onCustomHomePageInput(event) {
    if (this._telemetryHomePageTimer) {
      clearTimeout(this._telemetryHomePageTimer);
    }
    let browserHomePage = event.target.value;
    // The length of the home page URL string should be more then four,
    // and it should contain at least one ".", for example, "https://mozilla.org".
    if (browserHomePage.length > 4 && browserHomePage.includes(".")) {
      this._telemetryHomePageTimer = setTimeout(() => {
        let homePageNumber = browserHomePage.split("|").length;
        Services.telemetry.scalarAdd("preferences.browser_home_page_change", 1);
        Services.telemetry.keyedScalarAdd("preferences.browser_home_page_count", homePageNumber, 1);
      }, 3000);
    }
  },

  onCustomHomePageChange(event) {
    const homePref = Preferences.get("browser.startup.homepage");
    const value = event.target.value || homePref.defaultValue;
    homePref.value = value;
  },

  init() {
    // Event Listeners
    document.getElementById("homeMode").addEventListener("command", this.onMenuChange.bind(this));
    document.getElementById("homePageUrl").addEventListener("change", this.onCustomHomePageChange.bind(this));
    document.getElementById("homePageUrl").addEventListener("input", this.onCustomHomePageInput.bind(this));
    document.getElementById("useCurrentBtn").addEventListener("command", this.setHomePageToCurrent.bind(this));
    document.getElementById("useBookmarkBtn").addEventListener("command", this.setHomePageToBookmark.bind(this));
    document.getElementById("restoreDefaultHomePageBtn").addEventListener("command", this.restoreDefaultHomePage.bind(this));

    this._updateUseCurrentButton();
    window.addEventListener("focus", this._updateUseCurrentButton.bind(this));

    // Extension/override-related events
    this.watchNewTab();
    document.getElementById("disableHomePageExtension").addEventListener("command",
      makeDisableControllingExtension(PREF_SETTING_TYPE, HOMEPAGE_OVERRIDE_KEY));
    document.getElementById("disableNewTabExtension").addEventListener("command",
      makeDisableControllingExtension(URL_OVERRIDES_TYPE, NEW_TAB_KEY));
  }
};
