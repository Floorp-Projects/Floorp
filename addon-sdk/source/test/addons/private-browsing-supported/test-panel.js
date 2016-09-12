/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { open, focus, close } = require('sdk/window/helpers');
const { isPrivate } = require('sdk/private-browsing');
const { defer } = require('sdk/core/promise');
const { browserWindows: windows } = require('sdk/windows');
const { getInnerId, getMostRecentBrowserWindow } = require('sdk/window/utils');
const { getActiveView } = require('sdk/view/core');

const BROWSER = 'chrome://browser/content/browser.xul';

exports.testRequirePanel = function(assert) {
  require('sdk/panel');
  assert.ok('the panel module should not throw an error');
};

exports.testShowPanelInPrivateWindow = function(assert, done) {
  let panel = require('sdk/panel').Panel({
    contentURL: "data:text/html;charset=utf-8,I'm a leaf on the wind"
  });

  assert.ok(windows.length > 0, 'there is at least one open window');
  for (let window of windows) {
    assert.equal(isPrivate(window), false, 'open window is private');
  }

  let panelView = getActiveView(panel);
  let expectedWindowId = getInnerId(panelView.backgroundFrame.contentWindow);

  function checkPanelFrame() {
    let iframe = panelView.firstChild;

    assert.equal(panelView.viewFrame, iframe, 'panel has the correct viewFrame value');

    let windowId = getInnerId(iframe.contentWindow);

    assert.equal(windowId, expectedWindowId, 'panel has the correct window visible');

    assert.equal(iframe.contentDocument.body.textContent,
                 "I'm a leaf on the wind",
                 'the panel has the expected content');
  }

  function testPanel(window) {
    let { promise, resolve } = defer();

    assert.ok(!panel.isShowing, 'the panel is not showing [1]');

    panel.once('show', function() {
      assert.ok(panel.isShowing, 'the panel is showing');

      checkPanelFrame();

      panel.once('hide', function() {
        assert.ok(!panel.isShowing, 'the panel is not showing [2]');

        resolve(window);
      });

      panel.hide();
    });

    panel.show();

    return promise;
  };

  let initialWindow = getMostRecentBrowserWindow();

  testPanel(initialWindow).
    then(makeEmptyPrivateBrowserWindow).
    then(focus).
    then(function(window) {
      assert.equal(isPrivate(window), true, 'opened window is private');
      assert.pass('private window was focused');
      return window;
    }).
    then(testPanel).
    then(close).
    then(() => focus(initialWindow)).
    then(testPanel).
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
