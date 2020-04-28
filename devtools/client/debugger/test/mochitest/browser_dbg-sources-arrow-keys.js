/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test keyboard arrow behaviour
add_task(async function() {
  const dbg = await initDebugger("doc-sources.html", "simple1", "simple2", "nested-source", "long.js");

  await clickElement(dbg, "sourceDirectoryLabel", 3);
  await assertSourceCount(dbg, 8);

  // Right key on open dir
  await pressKey(dbg, "Right");
  await assertNodeIsFocused(dbg, 3);

  // Right key on closed dir
  await pressKey(dbg, "Right");
  await assertSourceCount(dbg, 8);
  await assertNodeIsFocused(dbg, 4);

  // Left key on a open dir
  await pressKey(dbg, "Left");
  await assertSourceCount(dbg, 8);
  await assertNodeIsFocused(dbg, 4);

  // Down key on a closed dir
  await pressKey(dbg, "Down");
  await assertNodeIsFocused(dbg, 4);

  // Right key on a source
  await pressKey(dbg, "Right");
  await assertNodeIsFocused(dbg, 4);

  // Down key on a source
  await waitForSourceCount(dbg, 9);
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
  await pressKey(dbg, "Left");
  await assertSourceCount(dbg, 8);
  await pressKey(dbg, "Left");
  await assertNodeIsFocused(dbg, 3);

  // Up Key at the top of the source tree
  await pressKey(dbg, "Up");
  await assertNodeIsFocused(dbg, 2);
});
