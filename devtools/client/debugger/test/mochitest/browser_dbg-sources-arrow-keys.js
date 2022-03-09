/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/**
 * Test keyboard arrow behaviour on the SourceTree with a nested folder
 * that we manually expand/collapse via arrow keys.
 */
add_task(async function() {
  const dbg = await initDebugger(
    "doc-sources.html",
    "simple1.js",
    "simple2.js",
    "nested-source.js",
    "long.js"
  );

  // Before clicking on the source label, no source is displayed
  await waitForSourcesInSourceTree(dbg, [], { noExpand: true });
  await clickElement(dbg, "sourceDirectoryLabel", 3);
  // Right after, all sources, but the nested one are displayed
  await waitForSourcesInSourceTree(
    dbg,
    ["doc-sources.html", "simple1.js", "simple2.js", "long.js"],
    { noExpand: true }
  );

  // Right key on open dir
  await pressKey(dbg, "Right");
  await assertNodeIsFocused(dbg, 3);

  // Right key on closed dir
  await pressKey(dbg, "Right");
  await assertNodeIsFocused(dbg, 4);

  // Left key on a open dir
  await pressKey(dbg, "Left");
  await assertNodeIsFocused(dbg, 4);

  // Down key on a closed dir
  await pressKey(dbg, "Down");
  await assertNodeIsFocused(dbg, 4);

  // Right key on a source
  // We are focused on the nested source and up to this point we still display only the 4 initial sources
  await waitForSourcesInSourceTree(
    dbg,
    ["doc-sources.html", "simple1.js", "simple2.js", "long.js"],
    { noExpand: true }
  );
  await pressKey(dbg, "Right");
  await assertNodeIsFocused(dbg, 4);
  // Now, the nested source is also displayed
  await waitForSourcesInSourceTree(
    dbg,
    [
      "doc-sources.html",
      "simple1.js",
      "simple2.js",
      "long.js",
      "nested-source.js",
    ],
    { noExpand: true }
  );

  // Down key on a source
  await pressKey(dbg, "Down");
  await assertNodeIsFocused(dbg, 5);

  // Go to bottom of tree and press down key
  await pressKey(dbg, "Down");
  await pressKey(dbg, "Down");
  await assertNodeIsFocused(dbg, 6);

  // Up key on a source
  await pressKey(dbg, "Up");
  await assertNodeIsFocused(dbg, 5);

  // Left key on a source
  await pressKey(dbg, "Left");
  await assertNodeIsFocused(dbg, 4);

  // Left key on a closed dir
  // We are about to close the nested folder, the nested source is about to disappear
  await waitForSourcesInSourceTree(
    dbg,
    [
      "doc-sources.html",
      "simple1.js",
      "simple2.js",
      "long.js",
      "nested-source.js",
    ],
    { noExpand: true }
  );
  await pressKey(dbg, "Left");
  // And it disappeared
  await waitForSourcesInSourceTree(
    dbg,
    ["doc-sources.html", "simple1.js", "simple2.js", "long.js"],
    { noExpand: true }
  );
  await pressKey(dbg, "Left");
  await assertNodeIsFocused(dbg, 3);

  // Up Key at the top of the source tree
  await pressKey(dbg, "Up");
  await assertNodeIsFocused(dbg, 2);
});
