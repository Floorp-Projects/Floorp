"use strict";

const URL = "data:text/html;charset=utf-8,<a href=%23>clickme</a>";

add_task(function* test_duplicate() {
  // Create new tab.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Flush to empty any queued update messages.
  yield TabStateFlusher.flush(browser);

  // Click the link to navigate, this will add second shistory entry.
  yield ContentTask.spawn(browser, null, function* () {
    return new Promise(resolve => {
      addEventListener("hashchange", function onHashChange() {
        removeEventListener("hashchange", onHashChange);
        resolve();
      });

      // Click the link.
      content.document.querySelector("a").click();
    });
  });

  // Duplicate the tab.
  let tab2 = ss.duplicateTab(window, tab);

  // Wait until the tab has fully restored.
  yield promiseTabRestored(tab2);
  yield TabStateFlusher.flush(tab2.linkedBrowser);

  // There should be two history entries now.
  let {entries} = JSON.parse(ss.getTabState(tab2));
  is(entries.length, 2, "there are two shistory entries");

  // Cleanup.
  yield promiseRemoveTab(tab2);
  yield promiseRemoveTab(tab);
});

add_task(function* test_duplicate_remove() {
  // Create new tab.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Flush to empty any queued update messages.
  yield TabStateFlusher.flush(browser);

  // Click the link to navigate, this will add second shistory entry.
  yield ContentTask.spawn(browser, null, function* () {
    return new Promise(resolve => {
      addEventListener("hashchange", function onHashChange() {
        removeEventListener("hashchange", onHashChange);
        resolve();
      });

      // Click the link.
      content.document.querySelector("a").click();
    });
  });

  // Duplicate the tab.
  let tab2 = ss.duplicateTab(window, tab);

  // Before the duplication finished, remove the tab.
  yield Promise.all([promiseRemoveTab(tab), promiseTabRestored(tab2)]);
  yield TabStateFlusher.flush(tab2.linkedBrowser);

  // There should be two history entries now.
  let {entries} = JSON.parse(ss.getTabState(tab2));
  is(entries.length, 2, "there are two shistory entries");

  // Cleanup.
  yield promiseRemoveTab(tab2);
});
