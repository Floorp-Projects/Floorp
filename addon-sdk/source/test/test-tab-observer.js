/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// TODO Fennec support in Bug #894525
module.metadata = {
  "engines": {
    "Firefox": "*"
  }
}

const { openTab, closeTab } = require("sdk/tabs/utils");
const { Loader } = require("sdk/test/loader");
const { setTimeout } = require("sdk/timers");

exports["test unload tab observer"] = function(assert, done) {
  let loader = Loader(module);

  let window = loader.require("sdk/deprecated/window-utils").activeBrowserWindow;
  let observer = loader.require("sdk/tabs/observer").observer;
  let opened = 0;
  let closed = 0;

  observer.on("open", function onOpen(window) { opened++; });
  observer.on("close", function onClose(window) { closed++; });

  // Open and close tab to trigger observers.
  closeTab(openTab(window, "data:text/html;charset=utf-8,tab-1"));

  // Unload the module so that all listeners set by observer are removed.
  loader.unload();

  // Open and close tab once again.
  closeTab(openTab(window, "data:text/html;charset=utf-8,tab-2"));

  // Enqueuing asserts to make sure that assertion is not performed early.
  setTimeout(function () {
    assert.equal(1, opened, "observer open was called before unload only");
    assert.equal(1, closed, "observer close was called before unload only");
    done();
  }, 0);
};

require("test").run(exports);

