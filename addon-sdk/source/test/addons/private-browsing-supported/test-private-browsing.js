/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Ci } = require('chrome');
const { isPrivateBrowsingSupported } = require('sdk/self');
const tabs = require('sdk/tabs');
const { browserWindows: windows } = require('sdk/windows');
const { isPrivate } = require('sdk/private-browsing');
const { is } = require('sdk/system/xul-app');
const { isWindowPBSupported, isTabPBSupported } = require('sdk/private-browsing/utils');
const { cleanUI } = require('sdk/test/utils');

const TAB_URL = 'about:addons';

exports.testIsPrivateBrowsingTrue = function(assert) {
  assert.ok(isPrivateBrowsingSupported,
            'isPrivateBrowsingSupported property is true');
};

// test that it is possible to open a private tab
exports.testTabOpenPrivate = function(assert, done) {
  tabs.open({
    url: TAB_URL,
    isPrivate: true,
    onReady: function(tab) {
      assert.equal(tab.url, TAB_URL, 'opened correct tab');
      assert.equal(isPrivate(tab), (isWindowPBSupported || isTabPBSupported), "tab is private");
      cleanUI().then(done).catch(console.exception);
    }
  });
}


// test that it is possible to open a non private tab
exports.testTabOpenPrivateDefault = function(assert, done) {
  tabs.open({
    url: TAB_URL,
    onReady: function(tab) {
      assert.equal(tab.url, TAB_URL, 'opened correct tab');
      assert.equal(isPrivate(tab), false, "tab is not private");
      cleanUI().then(done).catch(console.exception);
    }
  });
}

// test that it is possible to open a non private tab in explicit case
exports.testTabOpenPrivateOffExplicit = function(assert, done) {
  tabs.open({
    url: TAB_URL,
    isPrivate: false,
    onReady: function(tab) {
      assert.equal(tab.url, TAB_URL, 'opened correct tab');
      assert.equal(isPrivate(tab), false, "tab is not private");
      cleanUI().then(done).catch(console.exception);
    }
  });
}

// test windows.open with isPrivate: true
// test isPrivate on a window
if (!is('Fennec')) {
  // test that it is possible to open a private window
  exports.testWindowOpenPrivate = function(assert, done) {
    windows.open({
      url: TAB_URL,
      isPrivate: true,
      onOpen: function(window) {
        let tab = window.tabs[0];
        tab.once('ready', function() {
          assert.equal(tab.url, TAB_URL, 'opened correct tab');
          assert.equal(isPrivate(tab), isWindowPBSupported, 'tab is private');
          cleanUI().then(done).catch(console.exception);
        });
      }
    });
  };

  exports.testIsPrivateOnWindowOn = function(assert, done) {
    windows.open({
      isPrivate: true,
      onOpen: function(window) {
        assert.equal(isPrivate(window), isWindowPBSupported, 'isPrivate for a window is true when it should be');
        assert.equal(isPrivate(window.tabs[0]), isWindowPBSupported, 'isPrivate for a tab is false when it should be');
        cleanUI().then(done).catch(console.exception);
      }
    });
  };

  exports.testIsPrivateOnWindowOffImplicit = function(assert, done) {
    windows.open({
      onOpen: function(window) {
        assert.equal(isPrivate(window), false, 'isPrivate for a window is false when it should be');
        assert.equal(isPrivate(window.tabs[0]), false, 'isPrivate for a tab is false when it should be');
        cleanUI().then(done).catch(console.exception);
      }
    })
  }

  exports.testIsPrivateOnWindowOffExplicit = function(assert, done) {
    windows.open({
      isPrivate: false,
      onOpen: function(window) {
        assert.equal(isPrivate(window), false, 'isPrivate for a window is false when it should be');
        assert.equal(isPrivate(window.tabs[0]), false, 'isPrivate for a tab is false when it should be');
        cleanUI().then(done).catch(console.exception);
      }
    })
  }
}
