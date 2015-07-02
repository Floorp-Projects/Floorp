/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { onFocus, openDialog, open } = require('sdk/window/utils');
const { open: openPromise, close, focus, promise } = require('sdk/window/helpers');
const { isPrivate } = require('sdk/private-browsing');
const { getMode } = require('sdk/private-browsing/utils');
const { browserWindows: windows } = require('sdk/windows');
const { defer } = require('sdk/core/promise');
const tabs = require('sdk/tabs');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { cleanUI } = require("sdk/test/utils");

// test openDialog() from window/utils with private option
// test isActive state in pwpb case
// test isPrivate on ChromeWindow
exports.testPerWindowPrivateBrowsingGetter = function*(assert) {
  let win = openDialog({ private: true });

  yield promise(win, 'DOMContentLoaded');

  assert.equal(getMode(win), true, 'Newly opened window is in PB mode');
  assert.ok(isPrivate(win), 'isPrivate(window) is true');

  yield close(win);
}

// test open() from window/utils with private feature
// test isActive state in pwpb case
// test isPrivate on ChromeWindow
exports.testPerWindowPrivateBrowsingGetter = function*(assert) {
  let win = open('chrome://browser/content/browser.xul', {
    features: {
      private: true
    }
  });

  yield promise(win, 'DOMContentLoaded');
  assert.equal(getMode(win), true, 'Newly opened window is in PB mode');
  assert.ok(isPrivate(win), 'isPrivate(window) is true');
  yield close(win)
}

exports.testIsPrivateOnWindowOpen = function*(assert) {
  let window = yield new Promise(resolve => {
    windows.open({
      isPrivate: true,
      onOpen: resolve
    });
  });

  assert.equal(isPrivate(window), false, 'isPrivate for a window is true when it should be');
  assert.equal(isPrivate(window.tabs[0]), false, 'isPrivate for a tab is false when it should be');

  yield cleanUI();
}

exports.testIsPrivateOnWindowOpenFromPrivate = function(assert, done) {
    // open a private window
    openPromise(null, {
      features: {
        private: true,
        chrome: true,
        titlebar: true,
        toolbar: true
      }
    }).then(focus).then(function(window) {
      let { promise, resolve } = defer();

      assert.equal(isPrivate(window), true, 'the only open window is private');

      windows.open({
        url: 'about:blank',
        onOpen: function(w) {
          assert.equal(isPrivate(w), false, 'new test window is not private');
          w.close(function() resolve(window));
        }
      });

      return promise;
    }).then(close).
       then(done, assert.fail);
};

exports.testOpenTabWithPrivateWindow = function*(assert) {
  let window = getMostRecentBrowserWindow().OpenBrowserWindow({ private: true });

  assert.pass("loading new private window");

  yield promise(window, 'load').then(focus);

  assert.equal(isPrivate(window), true, 'the focused window is private');

  yield new Promise(resolve => tabs.open({
    url: 'about:blank',
    onOpen: (tab) => {
      assert.equal(isPrivate(tab), false, 'the opened tab is not private');
      tab.close(resolve);
    }
  }));

  yield close(window);
};

exports.testIsPrivateOnWindowOff = function(assert, done) {
  windows.open({
    onOpen: function(window) {
      assert.equal(isPrivate(window), false, 'isPrivate for a window is false when it should be');
      assert.equal(isPrivate(window.tabs[0]), false, 'isPrivate for a tab is false when it should be');
      window.close(done);
    }
  })
}
