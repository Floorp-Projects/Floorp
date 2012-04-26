"use strict";

// Helpers for managing the browser frame preferences.

const browserFrameHelpers = {
  'getEnabledPref': function() {
    try {
      return SpecialPowers.getBoolPref('dom.mozBrowserFramesEnabled');
    }
    catch(e) {
      return undefined;
    }
  },

  'getWhitelistPref': function() {
    try {
      return SpecialPowers.getCharPref('dom.mozBrowserFramesWhitelist');
    }
    catch(e) {
      return undefined;
    }
  },

  'getOOPDisabledPref': function() {
    try {
      return SpecialPowers.getBoolPref('dom.ipc.tabs.disabled');
    }
    catch(e) {
      return undefined;
    }
  },

  'setEnabledPref': function(enabled) {
    if (enabled !== undefined) {
      SpecialPowers.setBoolPref('dom.mozBrowserFramesEnabled', enabled);
    }
    else {
      SpecialPowers.clearUserPref('dom.mozBrowserFramesEnabled');
    }
  },

  'setWhitelistPref': function(whitelist) {
    if (whitelist !== undefined) {
      SpecialPowers.setCharPref('dom.mozBrowserFramesWhitelist', whitelist);
    }
    else {
      SpecialPowers.clearUserPref('dom.mozBrowserFramesWhitelist');
    }
  },

  'setOOPDisabledPref': function(value) {
    if (value !== undefined) {
      SpecialPowers.setBoolPref('dom.ipc.tabs.disabled', value);
    }
    else {
      SpecialPowers.clearUserPref('dom.ipc.tabs.disabled');
    }
  },

  'addToWhitelist': function() {
    var whitelist = browserFrameHelpers.getWhitelistPref();
    whitelist += ',  http://' + window.location.host + ',  ';
    browserFrameHelpers.setWhitelistPref(whitelist);
  },

  'restoreOriginalPrefs': function() {
    browserFrameHelpers.setEnabledPref(browserFrameHelpers.origEnabledPref);
    browserFrameHelpers.setWhitelistPref(browserFrameHelpers.origWhitelistPref);
    browserFrameHelpers.setOOPDisabledPref(browserFrameHelpers.origOOPDisabledPref);
  },

  'origEnabledPref': null,
  'origWhitelistPref': null,
  'origOOPDisabledPref': null,

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

// Set OOPDisabledPref to true, because none of these tests pass with OOP
// browser frames, at the moment.
browserFrameHelpers.setOOPDisabledPref(true);

addEventListener('unload', function() {
  browserFrameHelpers.restoreOriginalPrefs();
});
