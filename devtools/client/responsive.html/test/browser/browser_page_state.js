/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test page state to ensure page is not reloaded and session history is not
// modified.

const DUMMY_1_URL = "http://example.com/";
const TEST_URL = `${URL_ROOT}doc_page_state.html`;
const DUMMY_2_URL = "http://example.com/browser/";

add_task(function* () {
  // Load up a sequence of pages:
  // 0. DUMMY_1_URL
  // 1. TEST_URL
  // 2. DUMMY_2_URL
  let tab = yield addTab(DUMMY_1_URL);
  let browser = tab.linkedBrowser;
  yield load(browser, TEST_URL);
  yield load(browser, DUMMY_2_URL);

  // Check session history state
  let history = yield getSessionHistory(browser);
  is(history.index, 2, "At page 2 in history");
  is(history.entries.length, 3, "3 pages in history");
  is(history.entries[0].uri, DUMMY_1_URL, "Page 0 URL matches");
  is(history.entries[1].uri, TEST_URL, "Page 1 URL matches");
  is(history.entries[2].uri, DUMMY_2_URL, "Page 2 URL matches");

  // Go back one so we're at the test page
  yield back(browser);

  // Check session history state
  history = yield getSessionHistory(browser);
  is(history.index, 1, "At page 1 in history");
  is(history.entries.length, 3, "3 pages in history");
  is(history.entries[0].uri, DUMMY_1_URL, "Page 0 URL matches");
  is(history.entries[1].uri, TEST_URL, "Page 1 URL matches");
  is(history.entries[2].uri, DUMMY_2_URL, "Page 2 URL matches");

  // Click on content to set an altered state that would be lost on reload
  yield BrowserTestUtils.synthesizeMouseAtCenter("body", {}, browser);

  let { ui } = yield openRDM(tab);

  // Check color inside the viewport
  let color = yield spawnViewportTask(ui, {}, function* () {
    // eslint-disable-next-line mozilla/no-cpows-in-tests
    return content.getComputedStyle(content.document.body)
                  .getPropertyValue("background-color");
  });
  is(color, "rgb(0, 128, 0)",
     "Content is still modified from click in viewport");

  yield closeRDM(tab);

  // Check color back in the browser tab
  color = yield ContentTask.spawn(browser, {}, function* () {
    // eslint-disable-next-line mozilla/no-cpows-in-tests
    return content.getComputedStyle(content.document.body)
                  .getPropertyValue("background-color");
  });
  is(color, "rgb(0, 128, 0)",
     "Content is still modified from click in browser tab");

  // Check session history state
  history = yield getSessionHistory(browser);
  is(history.index, 1, "At page 1 in history");
  is(history.entries.length, 3, "3 pages in history");
  is(history.entries[0].uri, DUMMY_1_URL, "Page 0 URL matches");
  is(history.entries[1].uri, TEST_URL, "Page 1 URL matches");
  is(history.entries[2].uri, DUMMY_2_URL, "Page 2 URL matches");

  yield removeTab(tab);
});
