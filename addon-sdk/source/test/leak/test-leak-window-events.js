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

const { asyncWindowLeakTest } = require("./leak-utils.js");
const { Loader } = require("sdk/test/loader");
const { open } = require("sdk/window/utils");

exports["test window/events for leaks"] = function*(assert) {
  yield asyncWindowLeakTest(assert, _ => {
    return new Promise((resolve, reject) => {
      let loader = Loader(module);
      let { events } = loader.require("sdk/window/events");
      let { on, off } = loader.require("sdk/event/core");

      on(events, "data", function handler(e) {
        try {
          if (e.type === "load") {
            e.target.close();
          }
          else if (e.type === "close") {
            off(events, "data", handler);

            // Let asyncWindowLeakTest call loader.unload() after the
            // leak check.
            resolve(loader);
          }
        } catch (e) {
          reject(e);
        }
      });

      // Open a window.  This will trigger our data events.
      open();
    });
  });
};

exports["test window/events for leaks with existing window"] = function*(assert) {
  yield asyncWindowLeakTest(assert, _ => {
    return new Promise((resolve, reject) => {
      let loader = Loader(module);
      let w = open();
      w.addEventListener("load", function windowLoaded(evt) {
        w.removeEventListener("load", windowLoaded);
        let { events } = loader.require("sdk/window/events");
        w.addEventListener("DOMWindowClose", function windowClosed(evt) {
          w.removeEventListener("DOMWindowClose", windowClosed);
          resolve(loader);
        });
        w.close();
      });
    });
  });
};

require("sdk/test").run(exports);
