/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  engines: {
    "Firefox": "*"
  }
};

const { Loader } = require("sdk/test/loader");
const { open, getMostRecentBrowserWindow, getOuterId } = require("sdk/window/utils");
const { setTimeout } = require("sdk/timers");

exports["test browser events"] = function(assert, done) {
  let loader = Loader(module);
  let { events } = loader.require("sdk/browser/events");
  let { on, off } = loader.require("sdk/event/core");
  let actual = [];

  on(events, "data", function handler(e) {
    actual.push(e);
    if (e.type === "load") window.close();
    if (e.type === "close") {
      // Unload the module so that all listeners set by observer are removed.

      let [ ready, load, close ] = actual;

      assert.equal(ready.type, "DOMContentLoaded");
      assert.equal(ready.target, window, "window ready");

      assert.equal(load.type, "load");
      assert.equal(load.target, window, "window load");

      assert.equal(close.type, "close");
      assert.equal(close.target, window, "window load");

      // Note: If window is closed right after this GC won't have time
      // to claim loader and there for this listener, there for it's safer
      // to remove listener.
      off(events, "data", handler);
      loader.unload();
      done();
    }
  });

  // Open window and close it to trigger observers.
  let window = open();
};

exports["test browser events ignore other wins"] = function(assert, done) {
  let loader = Loader(module);
  let { events: windowEvents } = loader.require("sdk/window/events");
  let { events: browserEvents } = loader.require("sdk/browser/events");
  let { on, off } = loader.require("sdk/event/core");
  let actualBrowser = [];
  let actualWindow = [];

  function browserEventHandler(e) actualBrowser.push(e)
  on(browserEvents, "data", browserEventHandler);
  on(windowEvents, "data", function handler(e) {
    actualWindow.push(e);
    // Delay close so that if "load" is also emitted on `browserEvents`
    // `browserEventHandler` will be invoked.
    if (e.type === "load") setTimeout(window.close);
    if (e.type === "close") {
      assert.deepEqual(actualBrowser, [], "browser events were not triggered");
      let [ open, ready, load, close ] = actualWindow;

      assert.equal(open.type, "open");
      assert.equal(open.target, window, "window is open");



      assert.equal(ready.type, "DOMContentLoaded");
      assert.equal(ready.target, window, "window ready");

      assert.equal(load.type, "load");
      assert.equal(load.target, window, "window load");

      assert.equal(close.type, "close");
      assert.equal(close.target, window, "window load");


      // Note: If window is closed right after this GC won't have time
      // to claim loader and there for this listener, there for it's safer
      // to remove listener.
      off(windowEvents, "data", handler);
      off(browserEvents, "data", browserEventHandler);
      loader.unload();
      done();
    }
  });

  // Open window and close it to trigger observers.
  let window = open("data:text/html,not a browser");
};

require("sdk/test").run(exports);
