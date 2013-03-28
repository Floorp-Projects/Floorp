/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Ci } = require('chrome');
const { isPrivateBrowsingSupported } = require('sdk/self');
const tabs = require('sdk/tabs');
const { browserWindows: windows } = require('sdk/windows');
const { isPrivate } = require('sdk/private-browsing');
const { getOwnerWindow } = require('sdk/private-browsing/window/utils');
const { is } = require('sdk/system/xul-app');
const { isWindowPBSupported, isTabPBSupported } = require('sdk/private-browsing/utils');

const TAB_URL = 'data:text/html;charset=utf-8,TEST-TAB';

exports.testIsPrivateBrowsingTrue = function(assert) {
  assert.ok(isPrivateBrowsingSupported,
            'isPrivateBrowsingSupported property is true');
};

// test tab.open with isPrivate: true
// test isPrivate on a tab
// test getOwnerWindow on windows and tabs
exports.testGetOwnerWindow = function(assert, done) {
  let window = windows.activeWindow;
  let chromeWindow = getOwnerWindow(window);
  assert.ok(chromeWindow instanceof Ci.nsIDOMWindow, 'associated window is found');

  tabs.open({
    url: 'about:blank',
    isPrivate: true,
    onOpen: function(tab) {
      // test that getOwnerWindow works as expected
      if (is('Fennec')) {
        assert.notStrictEqual(chromeWindow, getOwnerWindow(tab));
        assert.ok(getOwnerWindow(tab) instanceof Ci.nsIDOMWindow);
      }
      else {
        if (isWindowPBSupported) {
          assert.notStrictEqual(chromeWindow,
                                getOwnerWindow(tab),
                                'associated window is not the same for window and window\'s tab');
        }
        else {
          assert.strictEqual(chromeWindow,
                            getOwnerWindow(tab),
                            'associated window is the same for window and window\'s tab');
        }
      }

      let pbSupported = isTabPBSupported || isWindowPBSupported;

      // test that the tab is private if it should be
      assert.equal(isPrivate(tab), pbSupported);
      assert.equal(isPrivate(getOwnerWindow(tab)), pbSupported);

      tab.close(function() done());
    }
  });
};

// test that it is possible to open a private tab
exports.testTabOpenPrivate = function(assert, done) {
  tabs.open({
    url: TAB_URL,
    isPrivate: true,
    onReady: function(tab) {
      assert.equal(tab.url, TAB_URL, 'opened correct tab');
      assert.equal(isPrivate(tab), (isWindowPBSupported || isTabPBSupported));

      tab.close(function() {
        done();
      });
    }
  });
}


// test that it is possible to open a non private tab
exports.testTabOpenPrivateDefault = function(assert, done) {
  tabs.open({
    url: TAB_URL,
    onReady: function(tab) {
      assert.equal(tab.url, TAB_URL, 'opened correct tab');
      assert.equal(isPrivate(tab), false);

      tab.close(function() {
        done();
      });
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
      assert.equal(isPrivate(tab), false);

      tab.close(function() {
        done();
      });
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

          window.close(function() {
            done();
          });
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
        window.close(done);
      }
    });
  };

  exports.testIsPrivateOnWindowOffImplicit = function(assert, done) {
    windows.open({
      onOpen: function(window) {
        assert.equal(isPrivate(window), false, 'isPrivate for a window is false when it should be');
        assert.equal(isPrivate(window.tabs[0]), false, 'isPrivate for a tab is false when it should be');
        window.close(done);
      }
    })
  }

  exports.testIsPrivateOnWindowOffExplicit = function(assert, done) {
    windows.open({
      isPrivate: false,
      onOpen: function(window) {
        assert.equal(isPrivate(window), false, 'isPrivate for a window is false when it should be');
        assert.equal(isPrivate(window.tabs[0]), false, 'isPrivate for a tab is false when it should be');
        window.close(done);
      }
    })
  }
}
