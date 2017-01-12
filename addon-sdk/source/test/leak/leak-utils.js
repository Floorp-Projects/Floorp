/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu, Ci } = require("chrome");
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { SelfSupportBackend } = Cu.import("resource:///modules/SelfSupportBackend.jsm", {});
const Startup = Cu.import("resource://gre/modules/sdk/system/Startup.js", {}).exports;

// Adapted from the SpecialPowers.exactGC() code.  We don't have a
// window to operate on so we cannot use the exact same logic.  We
// use 6 GC iterations here as that is what is needed to clean up
// the windows we have tested with.
function gc() {
  return new Promise(resolve => {
    Cu.forceGC();
    Cu.forceCC();
    let count = 0;
    function genGCCallback() {
      Cu.forceCC();
      return function() {
        if (++count < 5) {
          Cu.schedulePreciseGC(genGCCallback());
        } else {
          resolve();
        }
      }
    }

    Cu.schedulePreciseGC(genGCCallback());
  });
}

// Execute the given test function and verify that we did not leak windows
// in the process.  The test function must return a promise or be a generator.
// If the promise is resolved, or generator completes, with an sdk loader
// object then it will be unloaded after the memory measurements.
exports.asyncWindowLeakTest = function*(assert, asyncTestFunc) {

  // SelfSupportBackend periodically tries to open windows.  This can
  // mess up our window leak detection below, so turn it off.
  SelfSupportBackend.uninit();

  // Wait for the browser to finish loading.
  yield Startup.onceInitialized;

  // Track windows that are opened in an array of weak references.
  let weakWindows = [];
  function windowObserver(subject, topic) {
    let supportsWeak = subject.QueryInterface(Ci.nsISupportsWeakReference);
    if (supportsWeak) {
      weakWindows.push(Cu.getWeakReference(supportsWeak));
    }
  }
  Services.obs.addObserver(windowObserver, "domwindowopened", false);

  // Execute the body of the test.
  let testLoader = yield asyncTestFunc(assert);

  // Stop tracking new windows and attempt to GC any resources allocated
  // by the test body.
  Services.obs.removeObserver(windowObserver, "domwindowopened");
  yield gc();

  // Check to see if any of the windows we saw survived the GC.  We consider
  // these leaks.
  assert.ok(weakWindows.length > 0, "should see at least one new window");
  for (let i = 0; i < weakWindows.length; ++i) {
    assert.equal(weakWindows[i].get(), null, "window " + i + " should be GC'd");
  }

  // Finally, unload the test body's loader if it provided one.  We do this
  // after our leak detection to avoid free'ing things on unload.  Users
  // don't tend to unload their addons very often, so we want to find leaks
  // that happen while addons are in use.
  if (testLoader) {
    testLoader.unload();
  }
}
