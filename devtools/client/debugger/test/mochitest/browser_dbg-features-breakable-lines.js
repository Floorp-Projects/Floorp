/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const testServer = createVersionizedHttpTestServer(
  "examples/sourcemaps-reload-uncompressed"
);
const TEST_URL = testServer.urlFor("index.html");

// Assert the behavior of the gutter that grays out non-breakable lines
add_task(async function testBreakableLinesOverReloads() {
  const dbg = await initDebuggerWithAbsoluteURL(
    TEST_URL,
    "index.html",
    "script.js",
    "original.js"
  );

  info("Assert breakable lines of the first html page load");
  await assertBreakableLines(dbg, "index.html", 78, [
    ...getRange(16, 17),
    21,
    ...getRange(24, 25),
    30,
    36,
  ]);

  info("Assert breakable lines of the first original source file, original.js");
  // The length of original.js is longer than the test file
  // because the sourcemap replaces the content of the original file
  // and appends a few lines with a "WEBPACK FOOTER" comment
  // All the appended lines are empty lines or comments, so none of them are breakable.
  await assertBreakableLines(dbg, "original.js", 15, [
    ...getRange(1, 3),
    5,
    ...getRange(8, 10),
  ]);

  info("Assert breakable lines of the simple first load of script.js");
  await assertBreakableLines(dbg, "script.js", 9, [1, 5, 7, 8, 9]);

  info("Assert breakable lines of the first iframe page load");
  await assertBreakableLines(dbg, "iframe.html", 30, [
    ...getRange(16, 17),
    ...getRange(22, 23),
  ]);

  info(
    "Reload the page, wait for sources and assert that breakable lines get updated"
  );
  testServer.switchToNextVersion();
  await reload(dbg, "index.html", "script.js", "original.js", "iframe.html");

  // Wait for previously selected source to be re-selected after reload
  // otherwise it may overlap with the next source selected by assertBreakableLines.
  await waitForSelectedSource(dbg, "iframe.html");

  info("Assert breakable lines of the more complex second load of script.js");
  await assertBreakableLines(dbg, "script.js", 23, [2, ...getRange(13, 23)]);

  info("Assert breakable lines of the second html page load");
  await assertBreakableLines(dbg, "index.html", 33, [25, 27]);

  info("Assert breakable lines of the second orignal file");
  // See first assertion about original.js,
  // the size of original.js doesn't match the size of the test file
  await assertBreakableLines(dbg, "original.js", 18, [
    ...getRange(1, 3),
    ...getRange(8, 11),
    13,
  ]);

  await selectSource(dbg, "iframe.html");
  // When EFT is disabled, iframe.html is a regular source and the right content is displayed
  if (isEveryFrameTargetEnabled()) {
    is(
      getCM(dbg).getValue(),
      `Error: Incorrect contents fetched, please reload.`
    );
  }
  /**
   * Bug 1762381 - Can't assert breakable lines yet, because the iframe page content fails loading

  info("Assert breakable lines of the second iframe page load");
  await assertBreakableLines(dbg, "iframe.html", 27, [
    ...getRange(15, 17),
    ...getRange(21, 23),
  ]);
  */
});
