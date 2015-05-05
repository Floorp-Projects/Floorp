"use strict";

const URL = "data:text/html,<title>initial title</title>";

add_task(function* () {
  // Create a new tab.
  let tab = gBrowser.addTab(URL);
  yield promiseBrowserLoaded(tab.linkedBrowser);

  // Remove the tab.
  yield promiseRemoveTab(tab);

  // Check the title.
  let [{state: {entries}}] = JSON.parse(ss.getClosedTabData(window));
  is(entries[0].title, "initial title", "correct title");
});

add_task(function* () {
  // Create a new tab.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Flush to ensure we collected the initial title.
  TabState.flush(browser);

  // Set a new title.
  yield ContentTask.spawn(browser, null, function* () {
    content.document.title = "new title";
  });

  // Remove the tab.
  yield promiseRemoveTab(tab);

  // Check the title.
  let [{state: {entries}}] = JSON.parse(ss.getClosedTabData(window));
  is(entries[0].title, "new title", "correct title");
});
