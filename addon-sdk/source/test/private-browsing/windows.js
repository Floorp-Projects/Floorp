/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { pb, pbUtils } = require('./helper');
const { onFocus, openDialog, open } = require('sdk/window/utils');
const { open: openPromise, close, focus, promise } = require('sdk/window/helpers');
const { isPrivate } = require('sdk/private-browsing');
const { browserWindows: windows } = require('sdk/windows');
const { defer } = require('sdk/core/promise');
const tabs = require('sdk/tabs');

// test openDialog() from window/utils with private option
// test isActive state in pwpb case
// test isPrivate on ChromeWindow
exports.testPerWindowPrivateBrowsingGetter = function(assert, done) {
  let win = openDialog({
    private: true
  });

  promise(win, 'DOMContentLoaded').then(function onload() {
    assert.equal(pbUtils.getMode(win),
                 true, 'Newly opened window is in PB mode');
    assert.ok(isPrivate(win), 'isPrivate(window) is true');
    assert.equal(pb.isActive, false, 'PB mode is not active');

    close(win).then(function() {
      assert.equal(pb.isActive, false, 'PB mode is not active');
      done();
    });
  });
}

// test open() from window/utils with private feature
// test isActive state in pwpb case
// test isPrivate on ChromeWindow
exports.testPerWindowPrivateBrowsingGetter = function(assert, done) {
  let win = open('chrome://browser/content/browser.xul', {
    features: {
      private: true
    }
  });

  promise(win, 'DOMContentLoaded').then(function onload() {
    assert.equal(pbUtils.getMode(win),
                 true, 'Newly opened window is in PB mode');
    assert.ok(isPrivate(win), 'isPrivate(window) is true');
    assert.equal(pb.isActive, false, 'PB mode is not active');

    close(win).then(function() {
      assert.equal(pb.isActive, false, 'PB mode is not active');
      done();
    });
  });
}

exports.testIsPrivateOnWindowOpen = function(assert, done) {
  windows.open({
    isPrivate: true,
    onOpen: function(window) {
      assert.equal(isPrivate(window), false, 'isPrivate for a window is true when it should be');
      assert.equal(isPrivate(window.tabs[0]), false, 'isPrivate for a tab is false when it should be');
      window.close(done);
    }
  });
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

exports.testOpenTabWithPrivateWindow = function(assert, done) {
  function start() {
    openPromise(null, {
      features: {
        private: true,
        toolbar: true
      }
    }).then(focus).then(function(window) {
      let { promise, resolve } = defer();
      assert.equal(isPrivate(window), true, 'the focused window is private');

      tabs.open({
        url: 'about:blank',
        onOpen: function(tab) {
          assert.equal(isPrivate(tab), false, 'the opened tab is not private');
          // not closing this tab on purpose.. for now...
          // we keep this tab open because we closed all windows
          // and must keep a non-private window open at end of this test for next ones.
          resolve(window);
        }
      });

      return promise;
    }).then(close).then(done, assert.fail);
  }

  (function closeWindows() {
    if (windows.length > 0) {
      return windows.activeWindow.close(closeWindows);
    }
    assert.pass('all pre test windows have been closed');
    return start();
  })()
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
