/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the source tree works.

add_task(async function() {
  const dbg = await initDebugger("doc-sources.html", "simple1", "simple2", "nested-source", "long.js");
  const {
    selectors: { getSelectedSource },
    getState
  } = dbg;

  // Expand nodes and make sure more sources appear.
  await assertSourceCount(dbg, 2);
  await clickElement(dbg, "sourceDirectoryLabel", 2);

  await assertSourceCount(dbg, 7);
  await clickElement(dbg, "sourceDirectoryLabel", 3);
  await assertSourceCount(dbg, 8);

  const selected = waitForDispatch(dbg, "SET_SELECTED_LOCATION");
  await clickElement(dbg, "sourceNode", 4);
  await selected;
  await waitForSelectedSource(dbg);

  // Ensure the source file clicked is now focused
  await waitForElementWithSelector(dbg, ".sources-list .focused");

  const focusedNode = findElementWithSelector(dbg, ".sources-list .focused");
  const fourthNode = findElement(dbg, "sourceNode", 4);
  const selectedSource = getSelectedSource(getState()).url;

  ok(fourthNode.classList.contains("focused"), "4th node is focused");
  ok(selectedSource.includes("nested-source.js"), "nested-source is selected");
  await assertNodeIsFocused(dbg, 4);
  await waitForSelectedSource(dbg, "nested-source");

  // Make sure new sources appear in the list.
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const script = content.document.createElement("script");
    script.src = "math.min.js";
    content.document.body.appendChild(script);
  });

  await waitForSourceCount(dbg, 9);
  await assertNodeIsFocused(dbg, 4);
  is(
    getSourceNodeLabel(dbg, 7),
    "math.min.js",
    "math.min.js - The dynamic script exists"
  );
});
