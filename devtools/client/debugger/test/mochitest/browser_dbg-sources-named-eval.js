/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Make sure named eval sources appear in the Debugger
// and we show proper source text content

"use strict";

add_task(async function() {
  const dbg = await initDebugger(
    "doc-sources.html",
    "simple1.js",
    "simple2.js",
    "nested-source.js",
    "long.js"
  );

  info("Assert that all page sources appear in the source tree");
  await waitForSourcesInSourceTree(dbg, [
    "doc-sources.html",
    "simple1.js",
    "simple2.js",
    "nested-source.js",
    "long.js",
  ]);

  info(`>>> contentTask: evaluate evaled.js`);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.eval(
      `window.evaledFunc = function() {};
//# sourceURL=evaled.js
`
    );
  });

  info("Assert that the evaled source appear in the source tree");
  await waitForSourcesInSourceTree(dbg, [
    "doc-sources.html",
    "simple1.js",
    "simple2.js",
    "nested-source.js",
    "long.js",
    "evaled.js",
  ]);

  info("Wait for the evaled source");
  await waitForSource(dbg, "evaled.js");
  await selectSource(dbg, "evaled.js");

  assertTextContentOnLine(dbg, 1, "window.evaledFunc = function() {};");
});
