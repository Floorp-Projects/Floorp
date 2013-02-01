/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { Ci } = require('chrome');
const { open, backgroundify, windows,
        getXULWindow, getBaseWindow } = require('sdk/window/utils');
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

exports['test top window creation'] = function(assert) {
  let window = open('data:text/html;charset=utf-8,Hello top window');
  assert.ok(~windows().indexOf(window), 'window was opened');
  window.close();
};

exports['test new top window with options'] = function(assert) {
  let window = open('data:text/html;charset=utf-8,Hi custom top window', {
    name: 'test',
    features: { height: 100, width: 200, toolbar: true }
  });
  assert.ok(~windows().indexOf(window), 'window was opened');
  assert.equal(window.name, 'test', 'name was set');
  assert.equal(window.innerHeight, 100, 'height is set');
  assert.equal(window.innerWidth, 200, 'height is set');
  assert.equal(window.toolbar.visible, true, 'toolbar was set');
  window.close();
};

exports['test backgroundify'] = function(assert) {
  let window = open('data:text/html;charset=utf-8,backgroundy');
  assert.ok(~windows().indexOf(window),
            'window is in the list of windows');
  let backgroundy = backgroundify(window);
  assert.equal(backgroundy, window, 'backgroundify returs give window back');
  assert.ok(!~windows().indexOf(window),
            'backgroundifyied window is in the list of windows');
  window.close();
};

require('test').run(exports);
