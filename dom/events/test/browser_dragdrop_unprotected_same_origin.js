/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that drag and drop events are sent at the right time. Includes tests for
// dragging between windows and iframes, all of the same origin.
// dom.events.dataTransfer.protected.enabled = false

"use strict";

const OUTER_BASE_1 = "https://example.org/browser/dom/events/test/";
const OUTER_BASE_2 = "https://example.org/browser/dom/events/test/";

// iframe domains
const INNER_BASE_1 = OUTER_BASE_1;
const INNER_BASE_2 = OUTER_BASE_2;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.events.dataTransfer.protected.enabled", false]],
  });
  registerCleanupFunction(async function () {
    SpecialPowers.popPrefEnv();
  });

  await setup();
});

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/events/test/browser_dragdrop_impl.js",
  this
);

runTest = runDnd;
