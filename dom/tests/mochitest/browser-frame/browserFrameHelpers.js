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

  getOOPByDefaultPref: function() {
    return this._getBoolPref("dom.ipc.browser_frames.oop_by_default");
  },

  setOOPByDefaultPref: function(value) {
    return this._setBoolPref("dom.ipc.browser_frames.oop_by_default", value);
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
    this.setOOPByDefaultPref(this.origOOPByDefaultPref);
    this.setPageThumbsEnabledPref(this.origPageThumbsEnabledPref);
  },

  'origEnabledPref': null,
  'origWhitelistPref': null,
  'origOOPDisabledPref': null,
  'origOOPByDefaultPref': null,
  'origPageThumbsEnabledPref': null,

  // Two basically-empty pages from two different domains you can load.
  'emptyPage1': 'http://example.com' +
                window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/')) +
                '/file_empty.html',
  'emptyPage2': 'http://example.org' +
                window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/')) +
                '/file_empty.html',
  'focusPage': 'http://example.org' +
                window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/')) +
                '/file_focus.html',
};

browserFrameHelpers.origEnabledPref = browserFrameHelpers.getEnabledPref();
browserFrameHelpers.origWhitelistPref = browserFrameHelpers.getWhitelistPref();
browserFrameHelpers.origOOPDisabledPref = browserFrameHelpers.getOOPDisabledPref();
browserFrameHelpers.origOOPByDefaultPref = browserFrameHelpers.getOOPByDefaultPref();
browserFrameHelpers.origPageThumbsEnabledPref = browserFrameHelpers.getPageThumbsEnabledPref();

// Disable tab view; it seriously messes us up.
browserFrameHelpers.setPageThumbsEnabledPref(false);

// OOP by default, except on Windows, where OOP doesn't work.
browserFrameHelpers.setOOPByDefaultPref(true);
if (navigator.platform.indexOf('Win') != -1) {
  browserFrameHelpers.setOOPDisabledPref(true);
}

addEventListener('unload', function() {
  browserFrameHelpers.restoreOriginalPrefs();
});
