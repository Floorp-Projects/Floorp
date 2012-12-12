/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Helpers for managing the browser frame preferences.
"use strict";

const browserElementTestHelpers = {
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

  getIPCSecurityDisabledPref: function() {
    return this._getBoolPref("network.disable.ipc.security");
  },

  setIPCSecurityDisabledPref: function(value) {
    return this._setBoolPref("network.disable.ipc.security", value);
  },

  getPageThumbsEnabledPref: function() {
    return this._getBoolPref('browser.pageThumbs.enabled');
  },

  setPageThumbsEnabledPref: function(value) {
    this._setBoolPref('browser.pageThumbs.enabled', value);
  },

  addPermission: function() {
    SpecialPowers.addPermission("browser", true, document);
    this.tempPermissions.push(location.href)
  },

  removeAllTempPermissions: function() {
    for(var i = 0; i < this.tempPermissions.length; i++) {
      SpecialPowers.removePermission("browser", this.tempPermissions[i]);
    }
  },

  addPermissionForUrl: function(url) {
    SpecialPowers.addPermission("browser", true, url);
    this.tempPermissions.push(url);
  },

  restoreOriginalPrefs: function() {
    this.setEnabledPref(this.origEnabledPref);
    this.setOOPDisabledPref(this.origOOPDisabledPref);
    this.setOOPByDefaultPref(this.origOOPByDefaultPref);
    this.setPageThumbsEnabledPref(this.origPageThumbsEnabledPref);
    this.setIPCSecurityDisabledPref(this.origIPCSecurityPref);
    this.removeAllTempPermissions();
  },

  'origEnabledPref': null,
  'origOOPDisabledPref': null,
  'origOOPByDefaultPref': null,
  'origPageThumbsEnabledPref': null,
  'origIPCSecurityPref': null,
  'tempPermissions': [],

  // Some basically-empty pages from different domains you can load.
  'emptyPage1': 'http://example.com' +
                window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/')) +
                '/file_empty.html',
  'emptyPage2': 'http://example.org' +
                window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/')) +
                '/file_empty.html',
  'emptyPage3': 'http://test1.example.org' +
                window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/')) +
                '/file_empty.html',
  'focusPage': 'http://example.org' +
                window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/')) +
                '/file_focus.html',
};

browserElementTestHelpers.origEnabledPref = browserElementTestHelpers.getEnabledPref();
browserElementTestHelpers.origOOPDisabledPref = browserElementTestHelpers.getOOPDisabledPref();
browserElementTestHelpers.origOOPByDefaultPref = browserElementTestHelpers.getOOPByDefaultPref();
browserElementTestHelpers.origPageThumbsEnabledPref = browserElementTestHelpers.getPageThumbsEnabledPref();
browserElementTestHelpers.origIPCSecurityPref = browserElementTestHelpers.getIPCSecurityDisabledPref();

// Disable tab view; it seriously messes us up.
browserElementTestHelpers.setPageThumbsEnabledPref(false);

// Enable or disable OOP-by-default depending on the test's filename.  You can
// still force OOP on or off with <iframe mozbrowser remote=true/false>, at
// least until bug 756376 lands.
var oop = location.pathname.indexOf('_inproc_') == -1;
browserElementTestHelpers.setOOPByDefaultPref(oop);
browserElementTestHelpers.setOOPDisabledPref(false);

// Disable the networking security checks; our test harness just tests browser elements
// without sticking them in apps, and the security checks dislike that.
browserElementTestHelpers.setIPCSecurityDisabledPref(true);

addEventListener('unload', function() {
  browserElementTestHelpers.restoreOriginalPrefs();
});
