/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Loader } = require("sdk/test/loader");
const { open, close } = require("sdk/window/helpers");
const { browserWindows: windows } = require("sdk/windows");
const { isBrowser } = require('sdk/window/utils');
const app = require("sdk/system/xul-app");

exports["test unload window observer"] = function(assert, done) {
  // Hacky way to be able to create unloadable modules via makeSandboxedLoader.
  let loader = Loader(module);
  let observer = loader.require("sdk/windows/observer").observer;
  let opened = 0;
  let closed = 0;
  let windowsOpen = windows.length;

  observer.on("open", onOpen);
  observer.on("close", onClose);

  // On Fennec, only test that the module does not throw an error
  if (app.is("Fennec")) {
    assert.pass("Windows observer did not throw on Fennec");
    return cleanUp();
  }

  // Open window and close it to trigger observers.
  open().
    then(close).
    then(loader.unload).
    then(open).
    then(close).
    then(function() {
      // Enqueuing asserts to make sure that assertion is not performed early.
      assert.equal(1, opened, "observer open was called before unload only");
      assert.equal(windowsOpen + 1, closed, "observer close was called before unload only");
    }).
    then(cleanUp, assert.fail);

  function cleanUp () {
    observer.removeListener("open", onOpen);
    observer.removeListener("close", onClose);
    done();
  }

  function onOpen(window) {
    // Ignoring non-browser windows
    if (isBrowser(window))
      opened++;
  }
  function onClose(window) {
    // Ignore non-browser windows & already opened `activeWindow` (unload will
    // emit close on it even though it is not actually closed).
    if (isBrowser(window))
      closed++;
  }
};

require("test").run(exports);
