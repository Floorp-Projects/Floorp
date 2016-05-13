/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that searching again for nodes after they are removed or added from the
// DOM works correctly.

const TEST_URL = URL_ROOT + "doc_inspector_search.html";

add_task(function* () {
  let { inspector, testActor } = yield openInspectorForURL(TEST_URL);

  info("Searching for test node #d1");
  yield focusSearchBoxUsingShortcut(inspector.panelWin);
  yield synthesizeKeys(["#", "d", "1", "VK_RETURN"], inspector);

  yield inspector.search.once("search-result");
  assertHasResult(inspector, true);

  info("Removing node #d1");
  // Expect an inspector-updated event here, because removing #d1 causes the
  // breadcrumbs to update (since #d1 is displayed in it).
  let onUpdated = inspector.once("inspector-updated");
  yield mutatePage(inspector, testActor,
                   "document.getElementById(\"d1\").remove()");
  yield onUpdated;

  info("Pressing return button to search again for node #d1.");
  yield synthesizeKeys("VK_RETURN", inspector);

  yield inspector.search.once("search-result");
  assertHasResult(inspector, false);

  info("Emptying the field and searching for a node that doesn't exist: #d3");
  let keys = ["VK_BACK_SPACE", "VK_BACK_SPACE", "VK_BACK_SPACE", "#", "d", "3",
              "VK_RETURN"];
  yield synthesizeKeys(keys, inspector);

  yield inspector.search.once("search-result");
  assertHasResult(inspector, false);

  info("Create the #d3 node in the page");
  // No need to expect an inspector-updated event here, Creating #d3 isn't going
  // to update the breadcrumbs in any ways.
  yield mutatePage(inspector, testActor,
                   `document.getElementById("d2").insertAdjacentHTML(
                    "afterend", "<div id=d3></div>")`);

  info("Pressing return button to search again for node #d3.");
  yield synthesizeKeys("VK_RETURN", inspector);

  yield inspector.search.once("search-result");
  assertHasResult(inspector, true);

  // Catch-all event for remaining server requests when searching for the new
  // node.
  yield inspector.once("inspector-updated");
});

function* synthesizeKeys(keys, inspector) {
  if (typeof keys === "string") {
    keys = [keys];
  }

  for (let key of keys) {
    info("Synthesizing key " + key + " in the search box");
    let eventHandled = once(inspector.searchBox, "keypress", true);
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
    yield eventHandled;
    info("Waiting for the search query to complete");
    yield inspector.searchSuggestions._lastQuery;
  }
}

function assertHasResult(inspector, expectResult) {
  is(inspector.searchBox.classList.contains("devtools-no-search-result"),
     !expectResult,
     "There are" + (expectResult ? "" : " no") + " search results");
}

function* mutatePage(inspector, testActor, expression) {
  let onMutation = inspector.once("markupmutation");
  yield testActor.eval(expression);
  yield onMutation;
}
