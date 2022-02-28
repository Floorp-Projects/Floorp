/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Make sure named eval sources appear in the list.

"use strict";

add_task(async function() {
  const dbg = await initDebugger(
    "doc-sources.html",
    "simple1.js",
    "simple2.js",
    "nested-source.js",
    "long.js"
  );

  info(`>>> contentTask: evaluate evaled.js\n`);
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.eval("window.evaledFunc = function() {} //# sourceURL=evaled.js");
  });

  await waitForSourceCount(dbg, 3);
  ok(true);
});

async function waitForSourceCount(dbg, i) {
  // We are forced to wait until the DOM nodes appear because the
  // source tree batches its rendering.
  await waitUntil(() => {
    return findAllElements(dbg, "sourceNodes").length === i;
  }, `waiting for source count ${i}`);
}
