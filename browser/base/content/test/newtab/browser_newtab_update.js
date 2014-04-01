/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Checks that newtab is updated as its links change.
 */

function runTests() {
  if (NewTabUtils.allPages.updateScheduledForHiddenPages) {
    // Wait for dynamic updates triggered by the previous test to finish.
    yield whenPagesUpdated(null, true);
  }

  // First, start with an empty page.  setLinks will trigger a hidden page
  // update because it calls clearHistory.  We need to wait for that update to
  // happen so that the next time we wait for a page update below, we catch the
  // right update and not the one triggered by setLinks.
  //
  // Why this weird way of yielding?  First, these two functions don't return
  // promises, they call TestRunner.next when done.  Second, the point at which
  // setLinks is done is independent of when the page update will happen, so
  // calling whenPagesUpdated cannot wait until that time.
  setLinks([]);
  whenPagesUpdated(null, true);
  yield null;
  yield null;

  // Strategy: Add some visits, open a new page, check the grid, repeat.
  fillHistory([link(1)]);
  yield whenPagesUpdated(null, true);
  yield addNewTabPageTab();
  checkGrid("1,,,,,,,,");

  fillHistory([link(2)]);
  yield whenPagesUpdated(null, true);
  yield addNewTabPageTab();
  checkGrid("2,1,,,,,,,");

  fillHistory([link(1)]);
  yield whenPagesUpdated(null, true);
  yield addNewTabPageTab();
  checkGrid("1,2,,,,,,,");

  // Wait for fillHistory to add all links before waiting for an update
  yield fillHistory([link(2), link(3), link(4)], TestRunner.next);
  yield whenPagesUpdated(null, true);
  yield addNewTabPageTab();
  checkGrid("2,1,3,4,,,,,");
}

function link(id) {
  return { url: "http://example.com/#" + id, title: "site#" + id };
}
