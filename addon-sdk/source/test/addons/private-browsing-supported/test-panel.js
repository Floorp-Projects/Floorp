'use strict';

const { open, focus, close } = require('sdk/window/helpers');
const { isPrivate } = require('sdk/private-browsing');
const { defer } = require('sdk/core/promise');

const BROWSER = 'chrome://browser/content/browser.xul';

exports.testRequirePanel = function(assert) {
  require('panel');
  assert.ok('the panel module should not throw an error');
};

exports.testShowPanelInPrivateWindow = function(assert, done) {
  let panel = require('sdk/panel').Panel({
    contentURL: "data:text/html;charset=utf-8,"
  });

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
    then(done, assert.fail.bind(assert));
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

  assert.ok(!panel.isShowing, 'the panel is not showing [1]');

  panel.once('show', function() {
    assert.ok(panel.isShowing, 'the panel is showing');

    panel.once('hide', function() {
      assert.ok(!panel.isShowing, 'the panel is not showing [2]');

      resolve(null);
    });

    panel.hide();
  })
  panel.show();

  return promise;
}
