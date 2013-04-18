/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const windowUtils = require('sdk/deprecated/window-utils');
const { isWindowPBSupported, isGlobalPBSupported } = require('sdk/private-browsing/utils');
const { getFrames, getWindowTitle, onFocus, isWindowPrivate, windows, isBrowser } = require('sdk/window/utils');
const { open, close, focus } = require('sdk/window/helpers');
const { isPrivate } = require('sdk/private-browsing');
const { Panel } = require('sdk/panel');
const { Widget } = require('sdk/widget');
const { fromIterator: toArray } = require('sdk/util/array');

let { Loader } = require('sdk/test/loader');
let loader = Loader(module, {
  console: Object.create(console, {
    error: {
      value: function(e) !/DEPRECATED:/.test(e) ? console.error(e) : undefined
    }
  })
});
const pb = loader.require('sdk/private-browsing');

function makeEmptyBrowserWindow(options) {
  options = options || {};
  return open('chrome://browser/content/browser.xul', {
    features: {
      chrome: true,
      private: !!options.private,
      toolbar: true
    }
  });
}

exports.testShowPanelAndWidgetOnPrivateWindow = function(assert, done) {
  var myPrivateWindow;
  var finished = false;
  var privateWindow;
  var privateWindowClosed = false;

  pb.once('start', function() {
    assert.pass('private browsing mode started');

    // make a new private window
    makeEmptyBrowserWindow().then(function(window) {
      myPrivateWindow = window;

      let wt = windowUtils.WindowTracker({
        onTrack: function(window) {
          if (!isBrowser(window) || window !== myPrivateWindow) return;

          assert.ok(isWindowPrivate(window), 'window is private onTrack!');
          let panel = Panel({
            onShow: function() {
              assert.ok(this.isShowing, 'the panel is showing on the private window');

              let count = 0;
              let widget = Widget({
                id: "testShowPanelAndWidgetOnPrivateWindow-id",
                label: "My Hello Widget",
                content: "Hello!",
                onAttach: function(mod) {
                  count++;
                  if (count == 2) {
                    panel.destroy();
                    widget.destroy();
                    close(window);
                  }
                }
              });
            }
          }).show(null, window.gBrowser);
        },
        onUntrack: function(window) {
          if (window === myPrivateWindow) {
            wt.unload();

            pb.once('stop', function() {
              assert.pass('private browsing mode end');
              done();
            });

            pb.deactivate();
          }
        }
      });

      assert.equal(isWindowPrivate(window), true, 'the opened window is private');
      assert.equal(isPrivate(window), true, 'the opened window is private');
      assert.ok(getFrames(window).length > 1, 'there are frames for private window');
      assert.equal(getWindowTitle(window), window.document.title,
                   'getWindowTitle works');
    });
  });
  pb.activate();
};

exports.testWindowTrackerDoesNotIgnorePrivateWindows = function(assert, done) {
  var myPrivateWindow;
  var count = 0;

  let wt = windowUtils.WindowTracker({
    onTrack: function(window) {
      if (!isBrowser(window) || !isWindowPrivate(window)) return;
      assert.ok(isWindowPrivate(window), 'window is private onTrack!');
      if (++count == 1)
        close(window);
    },
    onUntrack: function(window) {
      if (count == 1 && isWindowPrivate(window)) {
        wt.unload();

        pb.once('stop', function() {
          assert.pass('private browsing mode end');
          done();
        });
        pb.deactivate();
      }
    }
  });

  pb.once('start', function() {
    assert.pass('private browsing mode started');
    makeEmptyBrowserWindow();
  });
  pb.activate();
}

exports.testWindowIteratorDoesNotIgnorePrivateWindows = function(assert, done) {
  pb.once('start', function() {
    // make a new private window
    makeEmptyBrowserWindow().then(function(window) {
      assert.ok(isWindowPrivate(window), "window is private");
      assert.equal(isPrivate(window), true, 'the opened window is private');
      assert.ok(toArray(windowUtils.windowIterator()).indexOf(window) > -1,
                "window is in windowIterator()");
      assert.ok(windows(null, { includePrivate: true }).indexOf(window) > -1,
                "window is in windows()");

      close(window).then(function() {
        pb.once('stop', function() {
          done();
        });
        pb.deactivate();
      });
    });
  });
  pb.activate();
};
