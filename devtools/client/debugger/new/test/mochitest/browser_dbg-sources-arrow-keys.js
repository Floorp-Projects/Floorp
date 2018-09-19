/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test keyboard arrow behaviour

async function waitForNodeToGainFocus(dbg, index) {
  await waitUntil(() => {
    const element = findElement(dbg, "sourceNode", index);

    if (element) {
      return element.classList.contains("focused");
    }

    return false;
  }, `waiting for source node ${index} to be focused`);
}

async function assertNodeIsFocused(dbg, index) {
  await waitForNodeToGainFocus(dbg, index);
  const node = findElement(dbg, "sourceNode", index);
  ok(node.classList.contains("focused"), `node ${index} is focused`);
}

add_task(async function() {
  const dbg = await initDebugger("doc-sources.html");
  await waitForSources(dbg, "simple1", "simple2", "nested-source", "long.js");

  await clickElement(dbg, "sourceDirectoryLabel", 2);
  await assertSourceCount(dbg, 7);

  // Right key on open dir
  await pressKey(dbg, "Right");
  await assertNodeIsFocused(dbg, 3);

  // Right key on closed dir
  await pressKey(dbg, "Right");
  await assertSourceCount(dbg, 8);
  await assertNodeIsFocused(dbg, 3);

  // Left key on a open dir
  await pressKey(dbg, "Left");
  await assertSourceCount(dbg, 7);
  await assertNodeIsFocused(dbg, 3);

  // Down key on a closed dir
  await pressKey(dbg, "Down");
  await assertNodeIsFocused(dbg, 4);

  // Right key on a source
  await pressKey(dbg, "Right");
  await assertNodeIsFocused(dbg, 5);

  // Down key on a source
  await pressKey(dbg, "Down");
  await assertNodeIsFocused(dbg, 6);

  // Go to bottom of tree and press down key
  await pressKey(dbg, "Down");
  await pressKey(dbg, "Down");
  await assertNodeIsFocused(dbg, 7);

  // Up key on a source
  await pressKey(dbg, "Up");
  await assertNodeIsFocused(dbg, 6);

  // Left key on a source
  await pressKey(dbg, "Left");
  await assertNodeIsFocused(dbg, 2);

  // Left key on a closed dir
  await pressKey(dbg, "Left");
  await assertSourceCount(dbg, 2);
  await pressKey(dbg, "Left");
  await assertNodeIsFocused(dbg, 1);

  // Up Key at the top of the source tree
  await pressKey(dbg, "Up");
  await assertNodeIsFocused(dbg, 1);
});
