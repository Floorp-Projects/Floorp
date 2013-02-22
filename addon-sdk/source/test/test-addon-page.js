/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { isTabOpen, activateTab, openTab,
        closeTab, getURI } = require('sdk/tabs/utils');
const windows = require('sdk/deprecated/window-utils');
const { Loader } = require('sdk/test/loader');
const { setTimeout } = require('sdk/timers');
const { is } = require('sdk/system/xul-app');
const tabs = require('sdk/tabs');

let uri = require('sdk/self').data.url('index.html');

function isChromeVisible(window) {
  let x = window.document.documentElement.getAttribute('disablechrome')
  return x !== 'true';
}

exports['test that add-on page has no chrome'] = function(assert, done) {
  let loader = Loader(module);
  loader.require('sdk/addon-page');

  let window = windows.activeBrowserWindow;
  let tab = openTab(window, uri);

  assert.ok(isChromeVisible(window), 'chrome is visible for non addon page');

  // need to do this in another turn to make sure event listener
  // that sets property has time to do that.
  setTimeout(function() {
    activateTab(tab);

    assert.equal(isChromeVisible(window), is('Fennec'), 'chrome is not visible for addon page');

    closeTab(tab);
    assert.ok(isChromeVisible(window), 'chrome is visible again');
    loader.unload();
    done();
  });
};

exports['test that add-on page with hash has no chrome'] = function(assert, done) {
  let loader = Loader(module);
  loader.require('sdk/addon-page');

  let window = windows.activeBrowserWindow;
  let tab = openTab(window, uri + "#foo");

  assert.ok(isChromeVisible(window), 'chrome is visible for non addon page');

  // need to do this in another turn to make sure event listener
  // that sets property has time to do that.
  setTimeout(function() {
    activateTab(tab);

    assert.equal(isChromeVisible(window), is('Fennec'), 'chrome is not visible for addon page');

    closeTab(tab);
    assert.ok(isChromeVisible(window), 'chrome is visible again');
    loader.unload();
    done();
  });
};

exports['test that add-on page with querystring has no chrome'] = function(assert, done) {
  let loader = Loader(module);
  loader.require('sdk/addon-page');

  let window = windows.activeBrowserWindow;
  let tab = openTab(window, uri + '?foo=bar');

  assert.ok(isChromeVisible(window), 'chrome is visible for non addon page');

  // need to do this in another turn to make sure event listener
  // that sets property has time to do that.
  setTimeout(function() {
    activateTab(tab);

    assert.equal(isChromeVisible(window), is('Fennec'), 'chrome is not visible for addon page');

    closeTab(tab);
    assert.ok(isChromeVisible(window), 'chrome is visible again');
    loader.unload();
    done();
  });
};

exports['test that add-on page with hash and querystring has no chrome'] = function(assert, done) {
  let loader = Loader(module);
  loader.require('sdk/addon-page');

  let window = windows.activeBrowserWindow;
  let tab = openTab(window, uri + '#foo?foo=bar');

  assert.ok(isChromeVisible(window), 'chrome is visible for non addon page');

  // need to do this in another turn to make sure event listener
  // that sets property has time to do that.
  setTimeout(function() {
    activateTab(tab);

    assert.equal(isChromeVisible(window), is('Fennec'), 'chrome is not visible for addon page');

    closeTab(tab);
    assert.ok(isChromeVisible(window), 'chrome is visible again');
    loader.unload();
    done();
  });
};

exports['test that malformed uri is not an addon-page'] = function(assert, done) {
  let loader = Loader(module);
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
  let loader = Loader(module);
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
