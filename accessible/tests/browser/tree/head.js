/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported testChildrenIds */

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
