/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the source tree works.

add_task(async function() {
  const dbg = await initDebugger(
    "doc-sources.html",
    "simple1",
    "simple2",
    "nested-source",
    "long.js"
  );
  const {
    selectors: { getSelectedSource },
    getState,
  } = dbg;

  // Expand nodes and make sure more sources appear.
  await assertSourceCount(dbg, 3);
  is(
    findElement(dbg, "sourceNode", 1).textContent.trim(),
    "Main Thread",
    "Main thread is labeled properly"
  );
  await clickElement(dbg, "sourceDirectoryLabel", 3);

  await assertSourceCount(dbg, 8);
  await clickElement(dbg, "sourceDirectoryLabel", 4);
  await assertSourceCount(dbg, 9);

  const selected = waitForDispatch(dbg.store, "SET_SELECTED_LOCATION");
  await clickElement(dbg, "sourceNode", 5);
  await selected;
  await waitForSelectedSource(dbg, "nested-source.js");

  // Ensure the source file clicked is now focused
  await waitForElementWithSelector(dbg, ".sources-list .focused");

  const focusedNode = findElementWithSelector(dbg, ".sources-list .focused");
  const fourthNode = findElement(dbg, "sourceNode", 5);
  const selectedSource = getSelectedSource().url;

  ok(fourthNode.classList.contains("focused"), "4th node is focused");
  ok(selectedSource.includes("nested-source.js"), "nested-source is selected");
  await assertNodeIsFocused(dbg, 5);

  // Make sure new sources appear in the list.
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const script = content.document.createElement("script");
    script.src = "math.min.js";
    content.document.body.appendChild(script);
  });

  await waitForSourceCount(dbg, 10);
  await assertNodeIsFocused(dbg, 5);
  is(
    getSourceNodeLabel(dbg, 8),
    "math.min.js",
    "math.min.js - The dynamic script exists"
  );
});
