# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

/**
 * Listeners for the DevEdition theme.  This adds an extra stylesheet
 * to browser.xul if a pref is set and no other themes are applied.
 */
let DevEdition = {
  _prefName: "browser.devedition.theme.enabled",
  _themePrefName: "general.skins.selectedSkin",
  _lwThemePrefName: "lightweightThemes.isThemeSelected",
  _devtoolsThemePrefName: "devtools.theme",

  styleSheetLocation: "chrome://browser/skin/devedition.css",
  styleSheet: null,

  init: function () {
    this._updateDevtoolsThemeAttribute();
    this._updateStyleSheet();

    // Listen for changes to all prefs except for complete themes.
    // No need for this since changing a complete theme requires a
    // restart.
    Services.prefs.addObserver(this._lwThemePrefName, this, false);
    Services.prefs.addObserver(this._prefName, this, false);
    Services.prefs.addObserver(this._devtoolsThemePrefName, this, false);
  },

  observe: function (subject, topic, data) {
    if (topic == "nsPref:changed") {
      if (data == this._devtoolsThemePrefName) {
        this._updateDevtoolsThemeAttribute();
      } else {
        this._updateStyleSheet();
      }
    }
  },

  _updateDevtoolsThemeAttribute: function() {
    // Set an attribute on root element to make it possible
    // to change colors based on the selected devtools theme.
    document.documentElement.setAttribute("devtoolstheme",
      Services.prefs.getCharPref(this._devtoolsThemePrefName));
  },

  _updateStyleSheet: function() {
    // Only try to apply the dev edition theme if it is preffered
    // on and there are no other themes applied.
    let lightweightThemeSelected = false;
    try {
      lightweightThemeSelected = Services.prefs.getBoolPref(this._lwThemePrefName);
    } catch(e) {}

    let defaultThemeSelected = false;
    try {
       defaultThemeSelected = Services.prefs.getCharPref(this._themePrefName) == "classic/1.0";
    } catch(e) {}

    let deveditionThemeEnabled = Services.prefs.getBoolPref(this._prefName) &&
      !lightweightThemeSelected && defaultThemeSelected;

    if (deveditionThemeEnabled && !this.styleSheet) {
      let styleSheetAttr = `href="${this.styleSheetLocation}" type="text/css"`;
      let styleSheet = this.styleSheet = document.createProcessingInstruction(
        'xml-stylesheet', styleSheetAttr);
      this.styleSheet.addEventListener("load", function onLoad() {
        styleSheet.removeEventListener("load", onLoad);
        ToolbarIconColor.inferFromText();
      });
      document.insertBefore(this.styleSheet, document.documentElement);
    } else if (!deveditionThemeEnabled && this.styleSheet) {
      this.styleSheet.remove();
      this.styleSheet = null;
      ToolbarIconColor.inferFromText();
    }
  },

  uninit: function () {
    Services.prefs.removeObserver(this._lwThemePrefName, this);
    Services.prefs.removeObserver(this._prefName, this);
    Services.prefs.removeObserver(this._devtoolsThemePrefName, this);
    this.styleSheet = null;
  }
};
