/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the source tree works.

"use strict";

add_task(async function() {
  const dbg = await initDebugger(
    "doc-sources.html",
    "simple1.js",
    "simple2.js",
    "nested-source.js",
    "long.js"
  );
  const {
    selectors: { getSelectedSource },
  } = dbg;

  // Expand nodes and make sure more sources appear.
  is(
    findElement(dbg, "sourceNode", 1).textContent.trim(),
    "Main Thread",
    "Main thread is labeled properly"
  );
  info("Before interacting with the source tree, no source are displayed");
  await waitForSourcesInSourceTree(dbg, [], { noExpand: true });
  await clickElement(dbg, "sourceDirectoryLabel", 3);
  info(
    "After clicking on the directory, all sources but the nested one are displayed"
  );
  await waitForSourcesInSourceTree(
    dbg,
    ["doc-sources.html", "simple1.js", "simple2.js", "long.js"],
    { noExpand: true }
  );

  await clickElement(dbg, "sourceDirectoryLabel", 4);
  info(
    "After clicing on the nested directory, the nested source is also displayed"
  );
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

  const selected = waitForDispatch(dbg.store, "SET_SELECTED_LOCATION");
  await clickElement(dbg, "sourceNode", 5);
  await selected;
  await waitForSelectedSource(dbg, "nested-source.js");

  // Ensure the source file clicked is now focused
  await waitForElementWithSelector(dbg, ".sources-list .focused");

  const fourthNode = findElement(dbg, "sourceNode", 5);
  const selectedSource = getSelectedSource().url;

  ok(fourthNode.classList.contains("focused"), "4th node is focused");
  ok(selectedSource.includes("nested-source.js"), "nested-source is selected");
  await assertNodeIsFocused(dbg, 5);

  // Make sure new sources appear in the list.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const script = content.document.createElement("script");
    script.src = "math.min.js";
    content.document.body.appendChild(script);
  });

  info("After adding math.min.js, we got a new source displayed");
  await waitForSourcesInSourceTree(
    dbg,
    [
      "doc-sources.html",
      "simple1.js",
      "simple2.js",
      "long.js",
      "nested-source.js",
      "math.min.js",
    ],
    { noExpand: true }
  );
  await assertNodeIsFocused(dbg, 5);
  is(
    getSourceNodeLabel(dbg, 8),
    "math.min.js",
    "math.min.js - The dynamic script exists"
  );
});
