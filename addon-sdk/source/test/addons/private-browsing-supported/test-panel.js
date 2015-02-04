/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { open, focus, close } = require('sdk/window/helpers');
const { isPrivate } = require('sdk/private-browsing');
const { defer } = require('sdk/core/promise');
const { browserWindows: windows } = require('sdk/windows');

const BROWSER = 'chrome://browser/content/browser.xul';

exports.testRequirePanel = function(assert) {
  require('sdk/panel');
  assert.ok('the panel module should not throw an error');
};

exports.testShowPanelInPrivateWindow = function(assert, done) {
  let panel = require('sdk/panel').Panel({
    contentURL: "data:text/html;charset=utf-8,"
  });

  assert.ok(windows.length > 0, 'there is at least one open window');
  for (let window of windows) {
    assert.equal(isPrivate(window), false, 'open window is private');
  }

  testShowPanel(assert, panel).
    then(makeEmptyPrivateBrowserWindow).
    then(focus).
    then(function(window) {
      assert.equal(isPrivate(window), true, 'opened window is private');
      assert.pass('private window was focused');
      return window;
    }).
    then(function(window) {
      let { promise, resolve } = defer();

      assert.ok(!panel.isShowing, 'the panel is not showing [1]');

      panel.once('show', function() {
        assert.ok(panel.isShowing, 'the panel is showing');

        panel.once('hide', function() {
          assert.ok(!panel.isShowing, 'the panel is not showing [2]');

          resolve(window);
        });

        panel.hide();
      });

      panel.show();

      return promise;
    }).
    then(close).
    then(done).
    then(null, assert.fail);
};


function makeEmptyPrivateBrowserWindow(options) {
  options = options || {};
  return open(BROWSER, {
    features: {
      chrome: true,
      toolbar: true,
      private: true
    }
  });
}

function testShowPanel(assert, panel) {
  let { promise, resolve } = defer();
  let shown = false;

  assert.ok(!panel.isShowing, 'the panel is not showing [1]');

  panel.once('hide', function() {
    assert.ok(!panel.isShowing, 'the panel is not showing [2]');
    assert.ok(shown, 'the panel was shown')

    resolve(null);
  });

  panel.once('show', function() {
    shown = true;

    assert.ok(panel.isShowing, 'the panel is showing');

    panel.hide();
  });

  panel.show();

  return promise;
}

//Test disabled because of bug 911071
module.exports = {}
