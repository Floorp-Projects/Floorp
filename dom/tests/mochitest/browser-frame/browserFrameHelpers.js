"use strict";

// Helpers for managing the browser frame preferences.

const browserFrameHelpers = {
  _getBoolPref: function(pref) {
    try {
      return SpecialPowers.getBoolPref(pref);
    }
    catch (e) {
      return undefined;
    }
  },

  _setBoolPref: function(pref, value) {
    if (value !== undefined) {
      SpecialPowers.setBoolPref(pref, value);
    }
    else {
      SpecialPowers.clearUserPref(pref);
    }
  },

  _getCharPref: function(pref) {
    try {
      return SpecialPowers.getCharPref(pref);
    }
    catch (e) {
      return undefined;
    }
  },

  _setCharPref: function(pref, value) {
    if (value !== undefined) {
      SpecialPowers.setCharPref(pref, value);
    }
    else {
      SpecialPowers.clearUserPref(pref);
    }
  },

  getEnabledPref: function() {
    return this._getBoolPref('dom.mozBrowserFramesEnabled');
  },

  setEnabledPref: function(value) {
    this._setBoolPref('dom.mozBrowserFramesEnabled', value);
  },

  getWhitelistPref: function() {
    return this._getCharPref('dom.mozBrowserFramesWhitelist');
  },

  setWhitelistPref: function(whitelist) {
    this._setCharPref('dom.mozBrowserFramesWhitelist', whitelist);
  },

  getOOPDisabledPref: function() {
    return this._getBoolPref('dom.ipc.tabs.disabled');
  },

  setOOPDisabledPref: function(value) {
    this._setBoolPref('dom.ipc.tabs.disabled', value);
  },

  getPageThumbsEnabledPref: function() {
    return this._getBoolPref('browser.pageThumbs.enabled');
  },

  setPageThumbsEnabledPref: function(value) {
    this._setBoolPref('browser.pageThumbs.enabled', value);
  },

  addToWhitelist: function() {
    var whitelist = this.getWhitelistPref();
    whitelist += ',  http://' + window.location.host + ',  ';
    this.setWhitelistPref(whitelist);
  },

  restoreOriginalPrefs: function() {
    this.setEnabledPref(this.origEnabledPref);
    this.setWhitelistPref(this.origWhitelistPref);
    this.setOOPDisabledPref(this.origOOPDisabledPref);
    this.setPageThumbsEnabledPref(this.origPageThumbsEnabledPref);
  },

  'origEnabledPref': null,
  'origWhitelistPref': null,
  'origOOPDisabledPref': null,
  'origPageThumbsEnabledPref': null,

  // Two basically-empty pages from two different domains you can load.
  'emptyPage1': 'http://example.com' +
                window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/')) +
                '/file_empty.html',
  'emptyPage2': 'http://example.org' +
                window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/')) +
                '/file_empty.html',
};

browserFrameHelpers.origEnabledPref = browserFrameHelpers.getEnabledPref();
browserFrameHelpers.origWhitelistPref = browserFrameHelpers.getWhitelistPref();
browserFrameHelpers.origOOPDisabledPref = browserFrameHelpers.getOOPDisabledPref();
browserFrameHelpers.origPageThumbsEnabledPref = browserFrameHelpers.getPageThumbsEnabledPref();

// Disable tab view; it seriously messes us up.
browserFrameHelpers.setPageThumbsEnabledPref(false);

// OOP must be disabled on Windows; it doesn't work there.  Enable it
// everywhere else.
if (navigator.platform.indexOf('Win') != -1) {
  browserFrameHelpers.setOOPDisabledPref(true);
}
else {
  browserFrameHelpers.setOOPDisabledPref(false);
}

addEventListener('unload', function() {
  browserFrameHelpers.restoreOriginalPrefs();
});
