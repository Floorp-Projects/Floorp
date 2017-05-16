"use strict";

const URL = "data:text/html,<title>initial title</title>";

add_task(async function() {
  // Create a new tab.
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  await promiseBrowserLoaded(tab.linkedBrowser);

  // Remove the tab.
  await promiseRemoveTab(tab);

  // Check the title.
  let [{state: {entries}}] = JSON.parse(ss.getClosedTabData(window));
  is(entries[0].title, "initial title", "correct title");
});

add_task(async function() {
  // Create a new tab.
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Flush to ensure we collected the initial title.
  await TabStateFlusher.flush(browser);

  // Set a new title.
  await ContentTask.spawn(browser, null, async function() {
    return new Promise(resolve => {
      addEventListener("DOMTitleChanged", function onTitleChanged() {
        removeEventListener("DOMTitleChanged", onTitleChanged);
        resolve();
      });

      content.document.title = "new title";
    });
  });

  // Remove the tab.
  await promiseRemoveTab(tab);

  // Check the title.
  let [{state: {entries}}] = JSON.parse(ss.getClosedTabData(window));
  is(entries[0].title, "new title", "correct title");
});
