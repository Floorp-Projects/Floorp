/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported waitForIFrameUpdates, spawnTestStates */

// Load the shared-head file first.
/* import-globals-from ../shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as promisified-events.js.
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR }
);

// A utility function to make sure the information of scroll position or visible
// area changes reach to out-of-process iframes.
async function waitForIFrameUpdates() {
  // Wait for two frames since the information is notified via asynchronous IPC
  // calls.
  await new Promise(resolve => requestAnimationFrame(resolve));
  await new Promise(resolve => requestAnimationFrame(resolve));
}

// A utility function to test the state of |elementId| element in out-of-process
// |browsingContext|.
async function spawnTestStates(browsingContext, elementId, expectedStates) {
  function testStates(id, expected) {
    const acc = SpecialPowers.Cc[
      "@mozilla.org/accessibilityService;1"
    ].getService(SpecialPowers.Ci.nsIAccessibilityService);
    const target = content.document.getElementById(id);
    let state = {};
    acc.getAccessibleFor(target).getState(state, {});
    if (expected === 0) {
      Assert.equal(state.value, expected);
    } else {
      Assert.ok(state.value & expected);
    }
  }
  await SpecialPowers.spawn(
    browsingContext,
    [elementId, expectedStates],
    testStates
  );
}
