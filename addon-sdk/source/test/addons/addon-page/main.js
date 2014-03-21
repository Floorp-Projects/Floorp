/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { isTabOpen, activateTab, openTab,
        closeTab, getTabURL, getWindowHoldingTab } = require('sdk/tabs/utils');
const windows = require('sdk/deprecated/window-utils');
const { LoaderWithHookedConsole } = require('sdk/test/loader');
const { setTimeout } = require('sdk/timers');
const app = require("sdk/system/xul-app");
const tabs = require('sdk/tabs');
const isAustralis = "gCustomizeMode" in windows.activeBrowserWindow;
const { set: setPref, get: getPref } = require("sdk/preferences/service");
const { PrefsTarget } = require("sdk/preferences/event-target");
const { defer } = require('sdk/core/promise');

const DEPRECATE_PREF = "devtools.errorconsole.deprecation_warnings";

let uri = require('sdk/self').data.url('index.html');

function closeTabPromise(tab) {
  let { promise, resolve } = defer();
  let url = getTabURL(tab);

  tabs.on('close', function onCloseTab(t) {
    if (t.url == url) {
      tabs.removeListener('close', onCloseTab);
      setTimeout(_ => resolve(tab))
    }
  });
  closeTab(tab);

  return promise;
}

function isChromeVisible(window) {
  let x = window.document.documentElement.getAttribute('disablechrome')
  return x !== 'true';
}

// Once Bug 903018 is resolved, just move the application testing to
// module.metadata.engines
if (app.is('Firefox')) {

exports['test add-on page deprecation message'] = function(assert, done) {
  let { loader, messages } = LoaderWithHookedConsole(module);

  loader.require('sdk/preferences/event-target').PrefsTarget({
    branchName: "devtools.errorconsole."
  }).on("deprecation_warnings", function() {
    if (!getPref(DEPRECATE_PREF, false)) {
      return undefined;
    }

    loader.require('sdk/addon-page');

    assert.equal(messages.length, 1, "only one error is dispatched");
    assert.equal(messages[0].type, "error", "the console message is an error");

    let msg = messages[0].msg;
    assert.ok(msg.indexOf("DEPRECATED") === 0,
              "The message is deprecation message");

    loader.unload();
    done();
    return undefined;
  });
  setPref(DEPRECATE_PREF, false);
  setPref(DEPRECATE_PREF, true);
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

    assert.equal(isChromeVisible(window), app.is('Fennec') || isAustralis,
      'chrome is not visible for addon page');

    closeTabPromise(tab).then(function() {
      assert.ok(isChromeVisible(window), 'chrome is visible again');
      loader.unload();
      assert.ok(!isTabOpen(tab), 'add-on page tab is closed on unload');
      done();
    }).then(null, assert.fail);
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

    assert.equal(isChromeVisible(window), app.is('Fennec') || isAustralis,
      'chrome is not visible for addon page');

    closeTabPromise(tab).then(function() {
      assert.ok(isChromeVisible(window), 'chrome is visible again');
      loader.unload();
      assert.ok(!isTabOpen(tab), 'add-on page tab is closed on unload');
      done();
    }).then(null, assert.fail);
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

    assert.equal(isChromeVisible(window), app.is('Fennec') || isAustralis,
      'chrome is not visible for addon page');

    closeTabPromise(tab).then(function() {
      assert.ok(isChromeVisible(window), 'chrome is visible again');
      loader.unload();
      assert.ok(!isTabOpen(tab), 'add-on page tab is closed on unload');
      done();
    }).then(null, assert.fail);
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

    assert.equal(isChromeVisible(window), app.is('Fennec') || isAustralis,
      'chrome is not visible for addon page');

    closeTabPromise(tab).then(function() {
      assert.ok(isChromeVisible(window), 'chrome is visible again');
      loader.unload();
      assert.ok(!isTabOpen(tab), 'add-on page tab is closed on unload');
      done();
    }).then(null, assert.fail);
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

    closeTabPromise(tab).then(function() {
      loader.unload();
      done();
    }).then(null, assert.fail);
  });
};

} else {
  exports['test unsupported'] = (assert) => assert.pass('This application is unsupported.');
}

require('sdk/test/runner').runTestsFromModule(module);
