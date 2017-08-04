/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported contentSpawnMutation, testChildrenIds */

// Load the shared-head file first.
/* import-globals-from ../shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as events.js.
loadScripts({ name: "common.js", dir: MOCHITESTS_DIR }, "events.js", "layout.js");

/*
 * A test function for comparing the IDs of an accessible's children
 * with an expected array of IDs.
 */
function testChildrenIds(acc, expectedIds) {
  let ids = arrayFromChildren(acc).map(child => getAccessibleDOMNodeID(child));
  Assert.deepEqual(ids, expectedIds,
    `Children for ${getAccessibleDOMNodeID(acc)} are wrong.`);
}

/*
 * This function spawns a content task and awaits expected mutation events from
 * aria-owns changes. It's good at catching events we did *not* expect. We do
 * this advancing the layout refresh to flush the relocations/insertions queue.
 */
async function contentSpawnMutation(browser, waitFor, func) {
  let onReorders = waitForEvents({ expected: waitFor.expected || [] });
  let unexpectedListener = new UnexpectedEvents(waitFor.unexpected || []);

  function tick() {
    // 100ms is an arbitrary positive number to advance the clock.
    // We don't need to advance the clock for a11y mutations, but other
    // tick listeners may depend on an advancing clock with each refresh.
    content.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils).advanceTimeAndRefresh(100);
  }

  // This stops the refreh driver from doing its regular ticks, and leaves
  // us in control.
  await ContentTask.spawn(browser, null, tick);

  // Perform the tree mutation.
  await ContentTask.spawn(browser, null, func);

  // Do one tick to flush our queue (insertions, relocations, etc.)
  await ContentTask.spawn(browser, null, tick);

  await onReorders;

  unexpectedListener.stop();

  // Go back to normal refresh driver ticks.
  await ContentTask.spawn(browser, null, function() {
    content.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils).restoreNormalRefresh();
  });
}
