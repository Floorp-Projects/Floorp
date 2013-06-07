/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { isTabOpen, activateTab, openTab,
        closeTab, getURI } = require('sdk/tabs/utils');
const windows = require('sdk/deprecated/window-utils');
const { LoaderWithHookedConsole } = require('sdk/test/loader');
const { setTimeout } = require('sdk/timers');
const { is } = require('sdk/system/xul-app');
const tabs = require('sdk/tabs');
const isAustralis = "gCustomizeMode" in windows.activeBrowserWindow;

let uri = require('sdk/self').data.url('index.html');

function isChromeVisible(window) {
  let x = window.document.documentElement.getAttribute('disablechrome')
  return x !== 'true';
}

exports['test add-on page deprecation message'] = function(assert) {
  let { loader, messages } = LoaderWithHookedConsole(module);
  loader.require('sdk/addon-page');

  assert.equal(messages.length, 1, "only one error is dispatched");
  assert.equal(messages[0].type, "error", "the console message is an error");

  let msg = messages[0].msg;

  assert.ok(msg.indexOf("DEPRECATED") === 0,
            "The message is deprecation message");

  loader.unload();
};

exports['test that add-on page has no chrome'] = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  loader.require('sdk/addon-page');

  let window = windows.activeBrowserWindow;
  let tab = openTab(window, uri);

  assert.ok(isChromeVisible(window), 'chrome is visible for non addon page');

  // need to do this in another turn to make sure event listener
  // that sets property has time to do that.
  setTimeout(function() {
    activateTab(tab);

    assert.equal(isChromeVisible(window), is('Fennec') || isAustralis,
      'chrome is not visible for addon page');

    closeTab(tab);
    assert.ok(isChromeVisible(window), 'chrome is visible again');
    loader.unload();
    done();
  });
};

exports['test that add-on page with hash has no chrome'] = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  loader.require('sdk/addon-page');

  let window = windows.activeBrowserWindow;
  let tab = openTab(window, uri + "#foo");

  assert.ok(isChromeVisible(window), 'chrome is visible for non addon page');

  // need to do this in another turn to make sure event listener
  // that sets property has time to do that.
  setTimeout(function() {
    activateTab(tab);

    assert.equal(isChromeVisible(window), is('Fennec') || isAustralis,
      'chrome is not visible for addon page');

    closeTab(tab);
    assert.ok(isChromeVisible(window), 'chrome is visible again');
    loader.unload();
    done();
  });
};

exports['test that add-on page with querystring has no chrome'] = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  loader.require('sdk/addon-page');

  let window = windows.activeBrowserWindow;
  let tab = openTab(window, uri + '?foo=bar');

  assert.ok(isChromeVisible(window), 'chrome is visible for non addon page');

  // need to do this in another turn to make sure event listener
  // that sets property has time to do that.
  setTimeout(function() {
    activateTab(tab);

    assert.equal(isChromeVisible(window), is('Fennec') || isAustralis,
      'chrome is not visible for addon page');

    closeTab(tab);
    assert.ok(isChromeVisible(window), 'chrome is visible again');
    loader.unload();
    done();
  });
};

exports['test that add-on page with hash and querystring has no chrome'] = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  loader.require('sdk/addon-page');

  let window = windows.activeBrowserWindow;
  let tab = openTab(window, uri + '#foo?foo=bar');

  assert.ok(isChromeVisible(window), 'chrome is visible for non addon page');

  // need to do this in another turn to make sure event listener
  // that sets property has time to do that.
  setTimeout(function() {
    activateTab(tab);

    assert.equal(isChromeVisible(window), is('Fennec') || isAustralis,
      'chrome is not visible for addon page');

    closeTab(tab);
    assert.ok(isChromeVisible(window), 'chrome is visible again');
    loader.unload();
    done();
  });
};

exports['test that malformed uri is not an addon-page'] = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  loader.require('sdk/addon-page');

  let window = windows.activeBrowserWindow;
  let tab = openTab(window, uri + 'anguage');

  // need to do this in another turn to make sure event listener
  // that sets property has time to do that.
  setTimeout(function() {
    activateTab(tab);

    assert.ok(isChromeVisible(window), 'chrome is visible for malformed uri');

    closeTab(tab);
    loader.unload();
    done();
  });
};

exports['test that add-on pages are closed on unload'] = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  loader.require('sdk/addon-page');

  tabs.open({
    url: uri,
    onReady: function listener(tab) {
      loader.unload();
      assert.ok(!isTabOpen(tab), 'add-on page tabs are closed on unload');

      done();
    }
  });
};

require('test').run(exports);
