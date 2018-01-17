/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the primary browser navigation UI to verify it's connected to the viewport.

const DUMMY_1_URL = "http://example.com/";
const TEST_URL = `${URL_ROOT}doc_page_state.html`;
const DUMMY_2_URL = "http://example.com/browser/";
const DUMMY_3_URL = "http://example.com/browser/devtools/";

add_task(async function () {
  // Load up a sequence of pages:
  // 0. DUMMY_1_URL
  // 1. TEST_URL
  // 2. DUMMY_2_URL
  let tab = await addTab(DUMMY_1_URL);
  let browser = tab.linkedBrowser;
  await load(browser, TEST_URL);
  await load(browser, DUMMY_2_URL);

  // Check session history state
  let history = await getSessionHistory(browser);
  is(history.index - 1, 2, "At page 2 in history");
  is(history.entries.length, 3, "3 pages in history");
  is(history.entries[0].url, DUMMY_1_URL, "Page 0 URL matches");
  is(history.entries[1].url, TEST_URL, "Page 1 URL matches");
  is(history.entries[2].url, DUMMY_2_URL, "Page 2 URL matches");

  // Go back one so we're at the test page
  await back(browser);

  // Check session history state
  history = await getSessionHistory(browser);
  is(history.index - 1, 1, "At page 1 in history");
  is(history.entries.length, 3, "3 pages in history");
  is(history.entries[0].url, DUMMY_1_URL, "Page 0 URL matches");
  is(history.entries[1].url, TEST_URL, "Page 1 URL matches");
  is(history.entries[2].url, DUMMY_2_URL, "Page 2 URL matches");

  await openRDM(tab);

  ok(browser.webNavigation.canGoBack, "Going back is allowed");
  ok(browser.webNavigation.canGoForward, "Going forward is allowed");
  is(browser.documentURI.spec, TEST_URL, "documentURI matches page 1");
  is(browser.contentTitle, "Page State Test", "contentTitle matches page 1");

  await forward(browser);

  ok(browser.webNavigation.canGoBack, "Going back is allowed");
  ok(!browser.webNavigation.canGoForward, "Going forward is not allowed");
  is(browser.documentURI.spec, DUMMY_2_URL, "documentURI matches page 2");
  is(browser.contentTitle, "mochitest index /browser/", "contentTitle matches page 2");

  await back(browser);
  await back(browser);

  ok(!browser.webNavigation.canGoBack, "Going back is not allowed");
  ok(browser.webNavigation.canGoForward, "Going forward is allowed");
  is(browser.documentURI.spec, DUMMY_1_URL, "documentURI matches page 0");
  is(browser.contentTitle, "mochitest index /", "contentTitle matches page 0");

  let receivedStatusChanges = new Promise(resolve => {
    let statusChangesSeen = 0;
    let statusChangesExpected = 2;
    let progressListener = {
      onStatusChange(webProgress, request, status, message) {
        info(message);
        if (++statusChangesSeen == statusChangesExpected) {
          gBrowser.removeProgressListener(progressListener);
          ok(true, `${statusChangesExpected} status changes while loading`);
          resolve();
        }
      }
    };
    gBrowser.addProgressListener(progressListener);
  });
  await load(browser, DUMMY_3_URL);
  await receivedStatusChanges;

  ok(browser.webNavigation.canGoBack, "Going back is allowed");
  ok(!browser.webNavigation.canGoForward, "Going forward is not allowed");
  is(browser.documentURI.spec, DUMMY_3_URL, "documentURI matches page 3");
  is(browser.contentTitle, "mochitest index /browser/devtools/",
     "contentTitle matches page 3");

  await closeRDM(tab);

  // Check session history state
  history = await getSessionHistory(browser);
  is(history.index - 1, 1, "At page 1 in history");
  is(history.entries.length, 2, "2 pages in history");
  is(history.entries[0].url, DUMMY_1_URL, "Page 0 URL matches");
  is(history.entries[1].url, DUMMY_3_URL, "Page 1 URL matches");

  await removeTab(tab);
});
