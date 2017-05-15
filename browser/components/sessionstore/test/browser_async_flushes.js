"use strict";

const URL = "data:text/html;charset=utf-8,<a href=%23>clickme</a>";

add_task(async function test_flush() {
  // Create new tab.
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Flush to empty any queued update messages.
  await TabStateFlusher.flush(browser);

  // There should be one history entry.
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 1, "there is a single history entry");

  // Click the link to navigate, this will add second shistory entry.
  await ContentTask.spawn(browser, null, async function() {
    return new Promise(resolve => {
      addEventListener("hashchange", function onHashChange() {
        removeEventListener("hashchange", onHashChange);
        resolve();
      });

      // Click the link.
      content.document.querySelector("a").click();
    });
  });

  // Flush to empty any queued update messages.
  await TabStateFlusher.flush(browser);

  // There should be two history entries now.
  ({entries} = JSON.parse(ss.getTabState(tab)));
  is(entries.length, 2, "there are two shistory entries");

  // Cleanup.
  gBrowser.removeTab(tab);
});

add_task(async function test_crash() {
  // Create new tab.
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Flush to empty any queued update messages.
  await TabStateFlusher.flush(browser);

  // There should be one history entry.
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 1, "there is a single history entry");

  // Click the link to navigate.
  await ContentTask.spawn(browser, null, async function() {
    return new Promise(resolve => {
      addEventListener("hashchange", function onHashChange() {
        removeEventListener("hashchange", onHashChange);
        resolve();
      });

      // Click the link.
      content.document.querySelector("a").click();
    });
  });

  // Crash the browser and flush. Both messages are async and will be sent to
  // the content process. The "crash" message makes it first so that we don't
  // get a chance to process the flush. The TabStateFlusher however should be
  // notified so that the flush still completes.
  let promise1 = BrowserTestUtils.crashBrowser(browser);
  let promise2 = TabStateFlusher.flush(browser);
  await Promise.all([promise1, promise2]);

  // The pending update should be lost.
  ({entries} = JSON.parse(ss.getTabState(tab)));
  is(entries.length, 1, "still only one history entry");

  // Cleanup.
  gBrowser.removeTab(tab);
});

add_task(async function test_remove() {
  // Create new tab.
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Flush to empty any queued update messages.
  await TabStateFlusher.flush(browser);

  // There should be one history entry.
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 1, "there is a single history entry");

  // Click the link to navigate.
  await ContentTask.spawn(browser, null, async function() {
    return new Promise(resolve => {
      addEventListener("hashchange", function onHashChange() {
        removeEventListener("hashchange", onHashChange);
        resolve();
      });

      // Click the link.
      content.document.querySelector("a").click();
    });
  });

  // Request a flush and remove the tab. The flush should still complete.
  await Promise.all([TabStateFlusher.flush(browser), promiseRemoveTab(tab)]);
})
