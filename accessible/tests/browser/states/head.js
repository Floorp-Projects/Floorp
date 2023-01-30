/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported waitForIFrameA11yReady, waitForIFrameUpdates, spawnTestStates, testVisibility */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as promisified-events.js.
/* import-globals-from ../../mochitest/states.js */
/* import-globals-from ../../mochitest/role.js */
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR },
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

// This is another version of addA11yLoadEvent for fission.
async function waitForIFrameA11yReady(iFrameBrowsingContext) {
  await SimpleTest.promiseFocus(window);

  await SpecialPowers.spawn(iFrameBrowsingContext, [], () => {
    return new Promise(resolve => {
      function waitForDocLoad() {
        SpecialPowers.executeSoon(() => {
          const acc = SpecialPowers.Cc[
            "@mozilla.org/accessibilityService;1"
          ].getService(SpecialPowers.Ci.nsIAccessibilityService);

          const accDoc = acc.getAccessibleFor(content.document);
          let state = {};
          accDoc.getState(state, {});
          if (state.value & SpecialPowers.Ci.nsIAccessibleStates.STATE_BUSY) {
            SpecialPowers.executeSoon(waitForDocLoad);
            return;
          }
          resolve();
        }, 0);
      }
      waitForDocLoad();
    });
  });
}

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
  function testStates(id, expected, unexpected) {
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
    Assert.ok(!(state.value & unexpected));
  }
  await SpecialPowers.spawn(
    browsingContext,
    [elementId, expectedStates],
    testStates
  );
}

function testVisibility(acc, shouldBeOffscreen, shouldBeInvisible) {
  const [states] = getStates(acc);
  let looksGood = shouldBeOffscreen == ((states & STATE_OFFSCREEN) != 0);
  looksGood &= shouldBeInvisible == ((states & STATE_INVISIBLE) != 0);
  return looksGood;
}
