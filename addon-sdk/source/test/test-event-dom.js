/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { openWindow, closeWindow } = require('./util');
const { Loader } = require('sdk/test/loader');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { Cc, Ci } = require('chrome');
const els = Cc["@mozilla.org/eventlistenerservice;1"].
            getService(Ci.nsIEventListenerService);

function countListeners(target, type) {
  let listeners = els.getListenerInfoFor(target, {});
  return listeners.filter(listener => listener.type == type).length;
}

exports['test window close clears listeners'] = function*(assert) {
  let window = yield openWindow();
  let loader = Loader(module);

  // Any element will do here
  let gBrowser = window.gBrowser;

  // Other parts of the app may be listening for this
  let windowListeners = countListeners(window, "DOMWindowClose");

  // We can assume we're the only ones using the test events
  assert.equal(countListeners(gBrowser, "TestEvent1"), 0, "Should be no listener for test event 1");
  assert.equal(countListeners(gBrowser, "TestEvent2"), 0, "Should be no listener for test event 2");

  let { open } = loader.require('sdk/event/dom');

  open(gBrowser, "TestEvent1");
  assert.equal(countListeners(window, "DOMWindowClose"), windowListeners + 1,
               "Should have added a DOMWindowClose listener");
  assert.equal(countListeners(gBrowser, "TestEvent1"), 1, "Should be a listener for test event 1");
  assert.equal(countListeners(gBrowser, "TestEvent2"), 0, "Should be no listener for test event 2");

  open(gBrowser, "TestEvent2");
  assert.equal(countListeners(window, "DOMWindowClose"), windowListeners + 1,
               "Should not have added another DOMWindowClose listener");
  assert.equal(countListeners(gBrowser, "TestEvent1"), 1, "Should be a listener for test event 1");
  assert.equal(countListeners(gBrowser, "TestEvent2"), 1, "Should be a listener for test event 2");

  window = yield closeWindow(window);

  assert.equal(countListeners(window, "DOMWindowClose"), windowListeners,
               "Should have removed a DOMWindowClose listener");
  assert.equal(countListeners(gBrowser, "TestEvent1"), 0, "Should be no listener for test event 1");
  assert.equal(countListeners(gBrowser, "TestEvent2"), 0, "Should be no listener for test event 2");

  loader.unload();
};

exports['test unload clears listeners'] = function(assert) {
  let window = getMostRecentBrowserWindow();
  let loader = Loader(module);

  // Any element will do here
  let gBrowser = window.gBrowser;

  // Other parts of the app may be listening for this
  let windowListeners = countListeners(window, "DOMWindowClose");

  // We can assume we're the only ones using the test events
  assert.equal(countListeners(gBrowser, "TestEvent1"), 0, "Should be no listener for test event 1");
  assert.equal(countListeners(gBrowser, "TestEvent2"), 0, "Should be no listener for test event 2");

  let { open } = loader.require('sdk/event/dom');

  open(gBrowser, "TestEvent1");
  assert.equal(countListeners(window, "DOMWindowClose"), windowListeners + 1,
               "Should have added a DOMWindowClose listener");
  assert.equal(countListeners(gBrowser, "TestEvent1"), 1, "Should be a listener for test event 1");
  assert.equal(countListeners(gBrowser, "TestEvent2"), 0, "Should be no listener for test event 2");

  open(gBrowser, "TestEvent2");
  assert.equal(countListeners(window, "DOMWindowClose"), windowListeners + 1,
               "Should not have added another DOMWindowClose listener");
  assert.equal(countListeners(gBrowser, "TestEvent1"), 1, "Should be a listener for test event 1");
  assert.equal(countListeners(gBrowser, "TestEvent2"), 1, "Should be a listener for test event 2");

  loader.unload();

  assert.equal(countListeners(window, "DOMWindowClose"), windowListeners,
               "Should have removed a DOMWindowClose listener");
  assert.equal(countListeners(gBrowser, "TestEvent1"), 0, "Should be no listener for test event 1");
  assert.equal(countListeners(gBrowser, "TestEvent2"), 0, "Should be no listener for test event 2");
};

require('sdk/test').run(exports);
