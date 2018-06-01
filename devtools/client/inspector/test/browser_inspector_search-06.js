/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that searching again for nodes after they are removed or added from the
// DOM works correctly.

const TEST_URL = URL_ROOT + "doc_inspector_search.html";

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URL);

  info("Searching for test node #d1");
  await focusSearchBoxUsingShortcut(inspector.panelWin);
  await synthesizeKeys(["#", "d", "1", "VK_RETURN"], inspector);

  await inspector.search.once("search-result");
  assertHasResult(inspector, true);

  info("Removing node #d1");
  // Expect an inspector-updated event here, because removing #d1 causes the
  // breadcrumbs to update (since #d1 is displayed in it).
  const onUpdated = inspector.once("inspector-updated");
  await mutatePage(inspector, testActor,
                   "document.getElementById(\"d1\").remove()");
  await onUpdated;

  info("Pressing return button to search again for node #d1.");
  await synthesizeKeys("VK_RETURN", inspector);

  await inspector.search.once("search-result");
  assertHasResult(inspector, false);

  info("Emptying the field and searching for a node that doesn't exist: #d3");
  const keys = ["VK_BACK_SPACE", "VK_BACK_SPACE", "VK_BACK_SPACE", "#", "d", "3",
                "VK_RETURN"];
  await synthesizeKeys(keys, inspector);

  await inspector.search.once("search-result");
  assertHasResult(inspector, false);

  info("Create the #d3 node in the page");
  // No need to expect an inspector-updated event here, Creating #d3 isn't going
  // to update the breadcrumbs in any ways.
  await mutatePage(inspector, testActor,
                   `document.getElementById("d2").insertAdjacentHTML(
                    "afterend", "<div id=d3></div>")`);

  info("Pressing return button to search again for node #d3.");
  await synthesizeKeys("VK_RETURN", inspector);

  await inspector.search.once("search-result");
  assertHasResult(inspector, true);

  // Catch-all event for remaining server requests when searching for the new
  // node.
  await inspector.once("inspector-updated");
});

async function synthesizeKeys(keys, inspector) {
  if (typeof keys === "string") {
    keys = [keys];
  }

  for (const key of keys) {
    info("Synthesizing key " + key + " in the search box");
    const eventHandled = once(inspector.searchBox, "keypress", true);
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
    await eventHandled;
    info("Waiting for the search query to complete");
    await inspector.searchSuggestions._lastQuery;
  }
}

function assertHasResult(inspector, expectResult) {
  is(inspector.searchBox.classList.contains("devtools-style-searchbox-no-match"),
     !expectResult,
     "There are" + (expectResult ? "" : " no") + " search results");
}

async function mutatePage(inspector, testActor, expression) {
  const onMutation = inspector.once("markupmutation");
  await testActor.eval(expression);
  await onMutation;
}
