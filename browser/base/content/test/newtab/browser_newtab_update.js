/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Checks that newtab is updated as its links change.
 */
add_task(function* () {
  // First, start with an empty page.  setLinks will trigger a hidden page
  // update because it calls clearHistory.  We need to wait for that update to
  // happen so that the next time we wait for a page update below, we catch the
  // right update and not the one triggered by setLinks.
  let updatedPromise = whenPagesUpdated();
  let setLinksPromise = setLinks([]);
  yield Promise.all([updatedPromise, setLinksPromise]);

  // Strategy: Add some visits, open a new page, check the grid, repeat.
  yield fillHistoryAndWaitForPageUpdate([1]);
  yield* addNewTabPageTab();
  yield* checkGrid("1,,,,,,,,");

  yield fillHistoryAndWaitForPageUpdate([2]);
  yield* addNewTabPageTab();
  yield* checkGrid("2,1,,,,,,,");

  yield fillHistoryAndWaitForPageUpdate([1]);
  yield* addNewTabPageTab();
  yield* checkGrid("1,2,,,,,,,");

  yield fillHistoryAndWaitForPageUpdate([2, 3, 4]);
  yield* addNewTabPageTab();
  yield* checkGrid("2,1,3,4,,,,,");

  // Make sure these added links have the right type
  let type = yield performOnCell(1, cell => { return cell.site.link.type });
  is(type, "history", "added link is history");
});

function fillHistoryAndWaitForPageUpdate(links) {
  let updatedPromise = whenPagesUpdated;
  let fillHistoryPromise = fillHistory(links.map(link));
  return Promise.all([updatedPromise, fillHistoryPromise]);
}

function link(id) {
  return { url: "http://example" + id + ".com/", title: "site#" + id };
}
