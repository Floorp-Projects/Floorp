/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that search label updated correctcly based on the search result.

const TEST_URL = URL_ROOT + "doc_inspector_search.html";

add_task(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URL);
  let { panelWin, searchResultsLabel } = inspector;

  info("Searching for test node #d1");
  // Expect the label shows 1 result
  yield focusSearchBoxUsingShortcut(panelWin);
  synthesizeKeys("#d1", panelWin);
  EventUtils.synthesizeKey("VK_RETURN", {}, panelWin);

  yield inspector.search.once("search-result");
  is(searchResultsLabel.textContent, "1 of 1");

  info("Click the clear button");
  // Expect the label is cleared after clicking the clear button.

  inspector.searchClearButton.click();
  is(searchResultsLabel.textContent, "");

  // Catch-all event for remaining server requests when searching for the new
  // node.
  yield inspector.once("inspector-updated");
});
