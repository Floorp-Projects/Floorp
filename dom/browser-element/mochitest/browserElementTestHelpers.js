/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Helpers for managing the browser frame preferences.
"use strict";

function _getPath() {
  return window.location.pathname
               .substring(0, window.location.pathname.lastIndexOf('/'))
               .replace("/priority", "");
}

const browserElementTestHelpers = {
  _getBoolPref: function(pref) {
    try {
      return SpecialPowers.getBoolPref(pref);
    }
    catch (e) {
      return undefined;
    }
  },

  _setPref: function(pref, value) {
    this.lockTestReady();
    if (value !== undefined && value !== null) {
      SpecialPowers.pushPrefEnv({'set': [[pref, value]]}, this.unlockTestReady.bind(this));
    } else {
      SpecialPowers.pushPrefEnv({'clear': [[pref]]}, this.unlockTestReady.bind(this));
    }
  },

  _testReadyLockCount: 0,
  _firedTestReady: false,
  lockTestReady: function() {
    this._testReadyLockCount++;
  },

  unlockTestReady: function() {
    this._testReadyLockCount--;
    if (this._testReadyLockCount == 0 && !this._firedTestReady) {
      this._firedTestReady = true;
      dispatchEvent(new Event("testready"));
    }
  },

  enableProcessPriorityManager: function() {
    this._setPref('dom.ipc.processPriorityManager.testMode', true);
    this._setPref('dom.ipc.processPriorityManager.enabled', true);
  },

  setEnabledPref: function(value) {
    this._setPref('dom.mozBrowserFramesEnabled', value);
  },

  getOOPByDefaultPref: function() {
    return this._getBoolPref("dom.ipc.browser_frames.oop_by_default");
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

  'tempPermissions': [],

  // Some basically-empty pages from different domains you can load.
  'emptyPage1': 'http://example.com' + _getPath() + '/file_empty.html',
  'emptyPage2': 'http://example.org' + _getPath() + '/file_empty.html',
  'emptyPage3': 'http://test1.example.org' + _getPath() + '/file_empty.html',
  'focusPage': 'http://example.org' + _getPath() + '/file_focus.html',
};

// Set some prefs:
//
//  * browser.pageThumbs.enabled: false
//
//    Disable tab view; it seriously messes us up.
//
//  * dom.ipc.browser_frames.oop_by_default
//
//    Enable or disable OOP-by-default depending on the test's filename.  You
//    can still force OOP on or off with <iframe mozbrowser remote=true/false>,
//    at least until bug 756376 lands.
//
//  * dom.ipc.tabs.disabled: false
//
//    Allow us to create OOP frames.  Even if they're not the default, some
//    "in-process" tests create OOP frames.
//
//  * network.disable.ipc.security: true
//
//    Disable the networking security checks; our test harness just tests
//    browser elements without sticking them in apps, and the security checks
//    dislike that.
//
//    Unfortunately setting network.disable.ipc.security to false before the
//    child process(es) created by this test have shut down can cause us to
//    assert and kill the child process.  That doesn't cause the tests to fail,
//    but it's still scary looking.  So we just set the pref to true and never
//    pop that value.  We'll rely on the tests which test IPC security to set
//    it to false.
//
//  * security.mixed_content.block_active_content: false
//
//    Disable mixed active content blocking, so that tests can confirm that mixed
//    content results in a broken security state.

(function() {
  var oop = location.pathname.indexOf('_inproc_') == -1;

  browserElementTestHelpers.lockTestReady();
  SpecialPowers.setBoolPref("network.disable.ipc.security", true);
  SpecialPowers.pushPrefEnv({set: [["browser.pageThumbs.enabled", false],
                                   ["dom.ipc.browser_frames.oop_by_default", oop],
                                   ["dom.ipc.tabs.disabled", false],
                                   ["security.mixed_content.block_active_content", false]]},
                            browserElementTestHelpers.unlockTestReady.bind(browserElementTestHelpers));
})();

addEventListener('unload', function() {
  browserElementTestHelpers.removeAllTempPermissions();
});

// Wait for the load event before unlocking the test-ready event.
browserElementTestHelpers.lockTestReady();
addEventListener('load', function() {
  SimpleTest.executeSoon(browserElementTestHelpers.unlockTestReady.bind(browserElementTestHelpers));
});
