/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

// Opening new windows in Fennec causes issues
module.metadata = {
  engines: {
    'Firefox': '*'
  }
};

const { Ci } = require('chrome');
const { open, windows, isBrowser,
        getXULWindow, getBaseWindow, getToplevelWindow, getMostRecentWindow,
        getMostRecentBrowserWindow } = require('sdk/window/utils');
const { close } = require('sdk/window/helpers');
const windowUtils = require('sdk/deprecated/window-utils');

exports['test get nsIBaseWindow from nsIDomWindow'] = function(assert) {
  let active = windowUtils.activeBrowserWindow;

  assert.ok(!(active instanceof Ci.nsIBaseWindow),
            'active window is not nsIBaseWindow');

  assert.ok(getBaseWindow(active) instanceof Ci.nsIBaseWindow,
            'base returns nsIBaseWindow');
};

exports['test get nsIXULWindow from nsIDomWindow'] = function(assert) {
  let active = windowUtils.activeBrowserWindow;
  assert.ok(!(active instanceof Ci.nsIXULWindow),
            'active window is not nsIXULWindow');
  assert.ok(getXULWindow(active) instanceof Ci.nsIXULWindow,
            'base returns nsIXULWindow');
};

exports['test getToplevelWindow'] = function(assert) {
  let active = windowUtils.activeBrowserWindow;
  assert.equal(getToplevelWindow(active), active,
               'getToplevelWindow of toplevel window returns the same window');
  assert.equal(getToplevelWindow(active.content), active,
               'getToplevelWindow of tab window returns the browser window');
  assert.ok(getToplevelWindow(active) instanceof Ci.nsIDOMWindow,
            'getToplevelWindow returns nsIDOMWindow');
};

exports['test top window creation'] = function(assert, done) {
  let window = open('data:text/html;charset=utf-8,Hello top window');
  assert.ok(~windows().indexOf(window), 'window was opened');

  // Wait for the window unload before ending test
  close(window).then(done);
};

exports['test new top window with options'] = function(assert, done) {
  let window = open('data:text/html;charset=utf-8,Hi custom top window', {
    name: 'test',
    features: { height: 100, width: 200, toolbar: true }
  });
  assert.ok(~windows().indexOf(window), 'window was opened');
  assert.equal(window.name, 'test', 'name was set');
  assert.equal(window.innerHeight, 100, 'height is set');
  assert.equal(window.innerWidth, 200, 'height is set');
  assert.equal(window.toolbar.visible, true, 'toolbar was set');

  // Wait for the window unload before ending test
  close(window).then(done);
};

exports['test new top window with various URIs'] = function(assert, done) {
  let msg = 'only chrome, resource and data uris are allowed';
  assert.throws(function () {
    open('foo');
  }, msg);
  assert.throws(function () {
    open('http://foo');
  }, msg);
  assert.throws(function () {
    open('https://foo');
  }, msg);
  assert.throws(function () {
    open('ftp://foo');
  }, msg);
  assert.throws(function () {
    open('//foo');
  }, msg);

  let chromeWindow = open('chrome://foo/content/');
  assert.ok(~windows().indexOf(chromeWindow), 'chrome URI works');

  let resourceWindow = open('resource://foo');
  assert.ok(~windows().indexOf(resourceWindow), 'resource URI works');

  // Wait for the window unload before ending test
  close(chromeWindow).then(close.bind(null, resourceWindow)).then(done);
};

exports.testIsBrowser = function(assert) {
  // dummy window, bad type
  assert.equal(isBrowser({ document: { documentElement: { getAttribute: function() {
    return 'navigator:browserx';
  }}}}), false, 'dummy object with correct stucture and bad type does not pass');

  assert.ok(isBrowser(getMostRecentBrowserWindow()), 'active browser window is a browser window');
  assert.ok(!isBrowser({}), 'non window is not a browser window');
  assert.ok(!isBrowser({ document: {} }), 'non window is not a browser window');
  assert.ok(!isBrowser({ document: { documentElement: {} } }), 'non window is not a browser window');
  assert.ok(!isBrowser(), 'no argument is not a browser window');
};

require('test').run(exports);
