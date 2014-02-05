/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cc, Ci } = require('chrome');
const { isPrivate } = require('sdk/private-browsing');
const { isWindowPBSupported } = require('sdk/private-browsing/utils');
const { onFocus, getMostRecentWindow, getWindowTitle, getInnerId,
        getFrames, windows, open: openWindow, isWindowPrivate } = require('sdk/window/utils');
const { open, close, focus, promise } = require('sdk/window/helpers');
const { browserWindows } = require("sdk/windows");
const winUtils = require("sdk/deprecated/window-utils");
const { fromIterator: toArray } = require('sdk/util/array');
const tabs = require('sdk/tabs');

const WM = Cc['@mozilla.org/appshell/window-mediator;1'].getService(Ci.nsIWindowMediator);

const BROWSER = 'chrome://browser/content/browser.xul';

function makeEmptyBrowserWindow(options) {
  options = options || {};
  return open(BROWSER, {
    features: {
      chrome: true,
      private: !!options.private
    }
  }).then(focus);
}

exports.testWindowTrackerIgnoresPrivateWindows = function(assert, done) {
  var myNonPrivateWindowId, myPrivateWindowId;
  var privateWindowClosed = false;
  var privateWindowOpened = false;
  var trackedWindowIds = [];

  let wt = winUtils.WindowTracker({
    onTrack: function(window) {
      let id = getInnerId(window);
      trackedWindowIds.push(id);
    },
    onUntrack: function(window) {
      let id = getInnerId(window);
      if (id === myPrivateWindowId) {
        privateWindowClosed = true;
      }

      if (id === myNonPrivateWindowId) {
        assert.equal(privateWindowClosed, true, 'private window was untracked');
        wt.unload();
        done();
      }
    }
  });

  // make a new private window
  makeEmptyBrowserWindow({ private: true }).then(function(window) {
    myPrivateWindowId = getInnerId(window);

    assert.ok(trackedWindowIds.indexOf(myPrivateWindowId) >= 0, 'private window was tracked');
    assert.equal(isPrivate(window), isWindowPBSupported, 'private window isPrivate');
    assert.equal(isWindowPrivate(window), isWindowPBSupported);
    assert.ok(getFrames(window).length > 1, 'there are frames for private window');
    assert.equal(getWindowTitle(window), window.document.title,
                 'getWindowTitle works');

    close(window).then(function() {
      assert.pass('private window was closed');

      makeEmptyBrowserWindow().then(function(window) {
        myNonPrivateWindowId = getInnerId(window);
        assert.notEqual(myPrivateWindowId, myNonPrivateWindowId, 'non private window was opened');
        close(window);
      });
    });
  });
};

// Test setting activeWIndow and onFocus for private windows
exports.testSettingActiveWindowDoesNotIgnorePrivateWindow = function(assert, done) {
  let browserWindow = WM.getMostRecentWindow("navigator:browser");
  let testSteps;

  assert.equal(winUtils.activeBrowserWindow, browserWindow,
               "Browser window is the active browser window.");
  assert.ok(!isPrivate(browserWindow), "Browser window is not private.");

  // make a new private window
  makeEmptyBrowserWindow({
    private: true
  }).then(function(window) {
    let continueAfterFocus = function(window) onFocus(window).then(nextTest);

    // PWPB case
    if (isWindowPBSupported) {
      assert.ok(isPrivate(window), "window is private");
      assert.notDeepEqual(winUtils.activeBrowserWindow, browserWindow);
    }
    // Global case
    else {
      assert.ok(!isPrivate(window), "window is not private");
    }

    assert.strictEqual(winUtils.activeBrowserWindow, window,
                 "Correct active browser window pb supported");
    assert.notStrictEqual(browserWindow, window,
                 "The window is not the old browser window");

    testSteps = [
      function() {
        // test setting a non private window
        continueAfterFocus(winUtils.activeWindow = browserWindow);
      },
      function() {
        assert.strictEqual(winUtils.activeWindow, browserWindow,
                           "Correct active window [1]");
        assert.strictEqual(winUtils.activeBrowserWindow, browserWindow,
                           "Correct active browser window [1]");

        // test focus(window)
        focus(window).then(nextTest);
      },
      function(w) {
        assert.strictEqual(w, window, 'require("sdk/window/helpers").focus on window works');
        assert.strictEqual(winUtils.activeBrowserWindow, window,
                           "Correct active browser window [2]");
        assert.strictEqual(winUtils.activeWindow, window,
                           "Correct active window [2]");

        // test setting a private window
        continueAfterFocus(winUtils.activeWindow = window);
      },
      function() {
        assert.deepEqual(winUtils.activeBrowserWindow, window,
                         "Correct active browser window [3]");
        assert.deepEqual(winUtils.activeWindow, window,
                         "Correct active window [3]");

        // just to get back to original state
        continueAfterFocus(winUtils.activeWindow = browserWindow);
      },
      function() {
        assert.deepEqual(winUtils.activeBrowserWindow, browserWindow,
                         "Correct active browser window when pb mode is supported [4]");
        assert.deepEqual(winUtils.activeWindow, browserWindow,
                         "Correct active window when pb mode is supported [4]");

        close(window).then(done);
      }
    ];

    function nextTest() {
      let args = arguments;
      if (testSteps.length) {
        require('sdk/timers').setTimeout(function() {
          (testSteps.shift()).apply(null, args);
        }, 0);
      }
    }
    nextTest();
  });
};

exports.testActiveWindowDoesNotIgnorePrivateWindow = function(assert, done) {
  // make a new private window
  makeEmptyBrowserWindow({
    private: true
  }).then(function(window) {
    // PWPB case
    if (isWindowPBSupported) {
      assert.equal(isPrivate(winUtils.activeWindow), true,
                   "active window is private");
      assert.equal(isPrivate(winUtils.activeBrowserWindow), true,
                   "active browser window is private");
      assert.ok(isWindowPrivate(window), "window is private");
      assert.ok(isPrivate(window), "window is private");

      // pb mode is supported
      assert.ok(
        isWindowPrivate(winUtils.activeWindow),
        "active window is private when pb mode is supported");
      assert.ok(
        isWindowPrivate(winUtils.activeBrowserWindow),
        "active browser window is private when pb mode is supported");
      assert.ok(isPrivate(winUtils.activeWindow),
                "active window is private when pb mode is supported");
      assert.ok(isPrivate(winUtils.activeBrowserWindow),
        "active browser window is private when pb mode is supported");
    }
    // Global case
    else {
      assert.equal(isPrivate(winUtils.activeWindow), false,
                   "active window is not private");
      assert.equal(isPrivate(winUtils.activeBrowserWindow), false,
                   "active browser window is not private");
      assert.equal(isWindowPrivate(window), false, "window is not private");
      assert.equal(isPrivate(window), false, "window is not private");
    }

    close(window).then(done);
  });
}

exports.testWindowIteratorIgnoresPrivateWindows = function(assert, done) {
  // make a new private window
  makeEmptyBrowserWindow({
    private: true
  }).then(function(window) {
    assert.equal(isWindowPrivate(window), isWindowPBSupported);
    assert.ok(toArray(winUtils.windowIterator()).indexOf(window) > -1,
              "window is in windowIterator()");

    close(window).then(done);
  });
};

// test that it is not possible to find a private window in
// windows module's iterator
exports.testWindowIteratorPrivateDefault = function(assert, done) {
  // there should only be one window open here, if not give us the
  // the urls
  if (browserWindows.length > 1) {
    for each (let tab in tabs) {
      assert.fail("TAB URL: " + tab.url);
    }
  }
  else {
    assert.equal(browserWindows.length, 1, 'only one window open');
  }

  open('chrome://browser/content/browser.xul', {
    features: {
      private: true,
      chrome: true
    }
  }).then(focus).then(function(window) {
    // test that there is a private window opened
    assert.equal(isPrivate(window), isWindowPBSupported, 'there is a private window open');
    assert.equal(isPrivate(winUtils.activeWindow), isWindowPBSupported);
    assert.equal(isPrivate(getMostRecentWindow()), isWindowPBSupported);
    assert.equal(isPrivate(browserWindows.activeWindow), isWindowPBSupported);

    assert.equal(browserWindows.length, 2, '2 windows open');
    assert.equal(windows(null, { includePrivate: true }).length, 2);

    close(window).then(done);
  });
};
