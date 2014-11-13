/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Checks that newtab is updated as its links change.
 */
function runTests() {
  // First, start with an empty page.  setLinks will trigger a hidden page
  // update because it calls clearHistory.  We need to wait for that update to
  // happen so that the next time we wait for a page update below, we catch the
  // right update and not the one triggered by setLinks.
  yield whenPagesUpdatedAnd(resolve => setLinks([], resolve));

  // Strategy: Add some visits, open a new page, check the grid, repeat.
  yield fillHistoryAndWaitForPageUpdate([1]);
  yield addNewTabPageTab();
  checkGrid("1,,,,,,,,");

  yield fillHistoryAndWaitForPageUpdate([2]);
  yield addNewTabPageTab();
  checkGrid("2,1,,,,,,,");

  yield fillHistoryAndWaitForPageUpdate([1]);
  yield addNewTabPageTab();
  checkGrid("1,2,,,,,,,");

  yield fillHistoryAndWaitForPageUpdate([2, 3, 4]);
  yield addNewTabPageTab();
  checkGrid("2,1,3,4,,,,,");

  // Make sure these added links have the right type
  is(getCell(1).site.link.type, "history", "added link is history");
}

function fillHistoryAndWaitForPageUpdate(links) {
  return whenPagesUpdatedAnd(resolve => fillHistory(links.map(link), resolve));
}

function whenPagesUpdatedAnd(promiseConstructor) {
  let promise1 = new Promise(whenPagesUpdated);
  let promise2 = new Promise(promiseConstructor);
  return Promise.all([promise1, promise2]).then(TestRunner.next);
}

function link(id) {
  return { url: "http://example" + id + ".com/", title: "site#" + id };
}
