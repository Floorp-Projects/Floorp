/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { gc } = require("./leak-utils");
const { Loader } = require("sdk/test/loader");
const { Cu } = require("chrome");
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

exports["test sdk/event/chrome does not leak when not referenced"] = function*(assert) {
  let loader = Loader(module);
  let { observe } = loader.require("sdk/event/chrome");
  let { on } = loader.require("sdk/event/core");

  let gotFooEvent = false;
  on(observe("test-foo"), "data", function(evt) {
    gotFooEvent = true;
  });

  let bar = observe("test-bar");
  let barPromise = new Promise(resolve => {
    on(bar, "data", function(evt) {
      assert.ok(!gotFooEvent, "should not have gotten test-foo event");
      resolve();
    });
  });

  // This should clear the test-foo observer channel because we are not
  // holding a reference to it above.
  yield gc();

  Services.obs.notifyObservers(null, "test-foo", null);
  Services.obs.notifyObservers(null, "test-bar", null);

  yield barPromise;

  loader.unload();
}

require("sdk/test").run(exports);
