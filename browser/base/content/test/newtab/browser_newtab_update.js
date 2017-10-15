/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Checks that newtab is updated as its links change.
 */
add_task(async function() {
  // First, start with an empty page.  setLinks will trigger a hidden page
  // update because it calls clearHistory.  We need to wait for that update to
  // happen so that the next time we wait for a page update below, we catch the
  // right update and not the one triggered by setLinks.
  let updatedPromise = whenPagesUpdated();
  let setLinksPromise = setLinks([]);
  await Promise.all([updatedPromise, setLinksPromise]);

  // Strategy: Add some visits, open a new page, check the grid, repeat.
  await fillHistoryAndWaitForPageUpdate([1]);
  await addNewTabPageTab();
  await checkGrid("1,,,,,,,,");

  await fillHistoryAndWaitForPageUpdate([2]);
  await addNewTabPageTab();
  await checkGrid("2,1,,,,,,,");

  await fillHistoryAndWaitForPageUpdate([1]);
  await addNewTabPageTab();
  await checkGrid("1,2,,,,,,,");

  await fillHistoryAndWaitForPageUpdate([2, 3, 4]);
  await addNewTabPageTab();
  await checkGrid("2,1,3,4,,,,,");

  // Make sure these added links have the right type
  let type = await performOnCell(1, cell => { return cell.site.link.type; });
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
