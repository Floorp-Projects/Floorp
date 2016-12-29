/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Listeners for the DevEdition theme.  This adds an extra stylesheet
 * to browser.xul if a pref is set and no other themes are applied.
 */
var DevEdition = {
  _devtoolsThemePrefName: "devtools.theme",
  styleSheetLocation: "chrome://browser/skin/devedition.css",
  styleSheet: null,
  initialized: false,

  get isStyleSheetEnabled() {
    return this.styleSheet && !this.styleSheet.sheet.disabled;
  },

  get isThemeCurrentlyApplied() {
    let theme = LightweightThemeManager.currentTheme;
    return theme && theme.id == "firefox-devedition@mozilla.org";
  },

  init() {
    this.initialized = true;
    Services.prefs.addObserver(this._devtoolsThemePrefName, this, false);
    Services.obs.addObserver(this, "lightweight-theme-styling-update", false);
    Services.obs.addObserver(this, "lightweight-theme-window-updated", false);
    this._updateDevtoolsThemeAttribute();

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
      if (newTheme && newTheme.id == "firefox-devedition@mozilla.org") {
        this._toggleStyleSheet(true);
      } else {
        this._toggleStyleSheet(false);
      }
    } else if (topic == "lightweight-theme-window-updated" && subject == window) {
      this._updateLWTBrightness();
    }

    if (topic == "nsPref:changed" && data == this._devtoolsThemePrefName) {
      this._updateDevtoolsThemeAttribute();
    }
  },

  _inferBrightness() {
    ToolbarIconColor.inferFromText();
    // Get an inverted full screen button if the dark theme is applied.
    if (this.isStyleSheetEnabled &&
        document.documentElement.getAttribute("devtoolstheme") == "dark") {
      document.documentElement.setAttribute("brighttitlebarforeground", "true");
    } else {
      document.documentElement.removeAttribute("brighttitlebarforeground");
    }
  },

  _updateLWTBrightness() {
    if (this.isThemeCurrentlyApplied) {
      let devtoolsTheme = Services.prefs.getCharPref(this._devtoolsThemePrefName);
      let textColor = devtoolsTheme == "dark" ? "bright" : "dark";
      document.documentElement.setAttribute("lwthemetextcolor", textColor);
    }
  },

  _updateDevtoolsThemeAttribute() {
    // Set an attribute on root element to make it possible
    // to change colors based on the selected devtools theme.
    let devtoolsTheme = Services.prefs.getCharPref(this._devtoolsThemePrefName);
    if (devtoolsTheme != "dark") {
      devtoolsTheme = "light";
    }
    document.documentElement.setAttribute("devtoolstheme", devtoolsTheme);
    this._updateLWTBrightness();
    this._inferBrightness();
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
      gBrowser.tabContainer._positionPinnedTabs();
      this._inferBrightness();
    }
  },

  _toggleStyleSheet(deveditionThemeEnabled) {
    let wasEnabled = this.isStyleSheetEnabled;
    if (deveditionThemeEnabled && !wasEnabled) {
      // The stylesheet may not have been created yet if it wasn't
      // needed on initial load.  Make it now.
      if (!this.styleSheet) {
        this.createStyleSheet();
      }
      this.styleSheet.sheet.disabled = false;
      this.refreshBrowserDisplay();
    } else if (!deveditionThemeEnabled && wasEnabled) {
      this.styleSheet.sheet.disabled = true;
      this.refreshBrowserDisplay();
    }
  },

  uninit() {
    Services.prefs.removeObserver(this._devtoolsThemePrefName, this);
    Services.obs.removeObserver(this, "lightweight-theme-styling-update", false);
    Services.obs.removeObserver(this, "lightweight-theme-window-updated", false);
    if (this.styleSheet) {
      this.styleSheet.removeEventListener("load", this);
    }
    this.styleSheet = null;
  }
};

// If the DevEdition theme is going to be applied in gBrowserInit.onLoad,
// then preload it now.  This prevents a flash of unstyled content where the
// normal theme is applied while the DevEdition stylesheet is loading.
if (!AppConstants.RELEASE_OR_BETA &&
    this != Services.appShell.hiddenDOMWindow && DevEdition.isThemeCurrentlyApplied) {
  DevEdition.createStyleSheet();
}
