/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Listeners for the compact theme.  This adds an extra stylesheet
 * to browser.xul if a pref is set and no other themes are applied.
 */
var CompactTheme = {
  styleSheetLocation: "chrome://browser/skin/compacttheme.css",
  styleSheet: null,
  initialized: false,

  get isStyleSheetEnabled() {
    return this.styleSheet && !this.styleSheet.sheet.disabled;
  },

  get isThemeCurrentlyApplied() {
    let theme = LightweightThemeManager.currentTheme;
    return theme && (
           theme.id == "firefox-compact-dark@mozilla.org" ||
           theme.id == "firefox-compact-light@mozilla.org");
  },

  init() {
    this.initialized = true;
    Services.obs.addObserver(this, "lightweight-theme-styling-update");

    if (this.isThemeCurrentlyApplied) {
      this._toggleStyleSheet(true);
    }
  },

  createStyleSheet() {
    let styleSheetAttr = `href="${this.styleSheetLocation}" type="text/css"`;
    this.styleSheet = document.createProcessingInstruction(
      "xml-stylesheet", styleSheetAttr);
    this.styleSheet.addEventListener("load", this);
    document.insertBefore(this.styleSheet, document.documentElement);
    this.styleSheet.sheet.disabled = true;
  },

  observe(subject, topic, data) {
    if (topic == "lightweight-theme-styling-update") {
      let newTheme = JSON.parse(data);
      if (newTheme && (
          newTheme.id == "firefox-compact-light@mozilla.org" ||
          newTheme.id == "firefox-compact-dark@mozilla.org")) {
        // We are using the theme ID on this object instead of always referencing
        // LightweightThemeManager.currentTheme in case this is a preview
        this._toggleStyleSheet(true);
      } else {
        this._toggleStyleSheet(false);
      }

    }
  },

  handleEvent(e) {
    if (e.type === "load") {
      this.styleSheet.removeEventListener("load", this);
      this.refreshBrowserDisplay();
    }
  },

  refreshBrowserDisplay() {
    // Don't touch things on the browser if gBrowserInit.onLoad hasn't
    // yet fired.
    if (this.initialized) {
      gBrowser.tabContainer.themeLayoutChanged();
    }
  },

  _toggleStyleSheet(enabled) {
    let wasEnabled = this.isStyleSheetEnabled;
    if (enabled) {
      // The stylesheet may not have been created yet if it wasn't
      // needed on initial load.  Make it now.
      if (!this.styleSheet) {
        this.createStyleSheet();
      }
      this.styleSheet.sheet.disabled = false;
      this.refreshBrowserDisplay();
    } else if (!enabled && wasEnabled) {
      this.styleSheet.sheet.disabled = true;
      this.refreshBrowserDisplay();
    }
  },

  uninit() {
    Services.obs.removeObserver(this, "lightweight-theme-styling-update");
    if (this.styleSheet) {
      this.styleSheet.removeEventListener("load", this);
    }
    this.styleSheet = null;
  }
};

// If the compact theme is going to be applied in gBrowserInit.onLoad,
// then preload it now.  This prevents a flash of unstyled content where the
// normal theme is applied while the compact theme stylesheet is loading.
if (AppConstants.INSTALL_COMPACT_THEMES &&
    this != Services.appShell.hiddenDOMWindow && CompactTheme.isThemeCurrentlyApplied) {
  CompactTheme.createStyleSheet();
}
