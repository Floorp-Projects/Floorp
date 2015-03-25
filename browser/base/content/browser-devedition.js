# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

/**
 * Listeners for the DevEdition theme.  This adds an extra stylesheet
 * to browser.xul if a pref is set and no other themes are applied.
 */
let DevEdition = {
  _devtoolsThemePrefName: "devtools.theme",
  styleSheetLocation: "chrome://browser/skin/devedition.css",
  styleSheet: null,

  get isThemeCurrentlyApplied() {
    let theme = LightweightThemeManager.currentTheme;
    return theme && theme.id == "firefox-devedition@mozilla.org";
  },

  init: function () {
    Services.prefs.addObserver(this._devtoolsThemePrefName, this, false);
    Services.obs.addObserver(this, "lightweight-theme-styling-update", false);
    this._updateDevtoolsThemeAttribute();

    if (this.isThemeCurrentlyApplied) {
      this._toggleStyleSheet(true);
    }
  },

  observe: function (subject, topic, data) {
    if (topic == "lightweight-theme-styling-update") {
      let newTheme = JSON.parse(data);
      if (newTheme && newTheme.id == "firefox-devedition@mozilla.org") {
        this._toggleStyleSheet(true);
      } else {
        this._toggleStyleSheet(false);
      }
    }

    if (topic == "nsPref:changed" && data == this._devtoolsThemePrefName) {
      this._updateDevtoolsThemeAttribute();
    }
  },

  _inferBrightness: function() {
    ToolbarIconColor.inferFromText();
    // Get an inverted full screen button if the dark theme is applied.
    if (this.styleSheet &&
        document.documentElement.getAttribute("devtoolstheme") == "dark") {
      document.documentElement.setAttribute("brighttitlebarforeground", "true");
    } else {
      document.documentElement.removeAttribute("brighttitlebarforeground");
    }
  },

  _updateDevtoolsThemeAttribute: function() {
    // Set an attribute on root element to make it possible
    // to change colors based on the selected devtools theme.
    let devtoolsTheme = Services.prefs.getCharPref(this._devtoolsThemePrefName);
    if (devtoolsTheme != "dark") {
      devtoolsTheme = "light";
    }
    document.documentElement.setAttribute("devtoolstheme", devtoolsTheme);
    this._inferBrightness();
  },

  handleEvent: function(e) {
    if (e.type === "load") {
      this.styleSheet.removeEventListener("load", this);
      gBrowser.tabContainer._positionPinnedTabs();
      this._inferBrightness();
    }
  },

  _toggleStyleSheet: function(deveditionThemeEnabled) {
    if (deveditionThemeEnabled && !this.styleSheet) {
      let styleSheetAttr = `href="${this.styleSheetLocation}" type="text/css"`;
      this.styleSheet = document.createProcessingInstruction(
        'xml-stylesheet', styleSheetAttr);
      this.styleSheet.addEventListener("load", this);
      document.insertBefore(this.styleSheet, document.documentElement);
      // NB: we'll notify observers once the stylesheet has fully loaded, see
      // handleEvent above.
    } else if (!deveditionThemeEnabled && this.styleSheet) {
      this.styleSheet.removeEventListener("load", this);
      this.styleSheet.remove();
      this.styleSheet = null;
      gBrowser.tabContainer._positionPinnedTabs();
      this._inferBrightness();
    }
  },

  uninit: function () {
    Services.prefs.removeObserver(this._devtoolsThemePrefName, this);
    Services.obs.removeObserver(this, "lightweight-theme-styling-update", false);
    if (this.styleSheet) {
      this.styleSheet.removeEventListener("load", this);
    }
    this.styleSheet = null;
  }
};
