/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Opening new windows in Fennec causes issues
module.metadata = {
  engines: {
    'Firefox': '*'
  }
};

const { Loader } = require("sdk/test/loader");
const { open, getMostRecentBrowserWindow, getOuterId } = require("sdk/window/utils");

exports["test browser events"] = function(assert, done) {
  let loader = Loader(module);
  let { events } = loader.require("sdk/window/events");
  let { on, off } = loader.require("sdk/event/core");
  let actual = [];

  on(events, "data", function handler(e) {
    actual.push(e);
    if (e.type === "load") window.close();
    if (e.type === "close") {
      let [ open, ready, load, close ] = actual;
      assert.equal(open.type, "open")
      assert.equal(open.target, window, "window is open")

      assert.equal(ready.type, "DOMContentLoaded")
      assert.equal(ready.target, window, "window ready")

      assert.equal(load.type, "load")
      assert.equal(load.target, window, "window load")

      assert.equal(close.type, "close")
      assert.equal(close.target, window, "window load")

      // Note: If window is closed right after this GC won't have time
      // to claim loader and there for this listener. It's better to remove
      // remove listener here to avoid race conditions.
      off(events, "data", handler);
      loader.unload();
      done();
    }
  });

  // Open window and close it to trigger observers.
  let window = open();
};

require("sdk/test").run(exports);
