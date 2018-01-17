/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test page state to ensure page is not reloaded and session history is not
// modified.

const DUMMY_1_URL = "http://example.com/";
const TEST_URL = `${URL_ROOT}doc_page_state.html`;
const DUMMY_2_URL = "http://example.com/browser/";

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

  // Click on content to set an altered state that would be lost on reload
  await BrowserTestUtils.synthesizeMouseAtCenter("body", {}, browser);

  let { ui } = await openRDM(tab);

  // Check color inside the viewport
  let color = await spawnViewportTask(ui, {}, function () {
    // eslint-disable-next-line mozilla/no-cpows-in-tests
    return content.getComputedStyle(content.document.body)
                  .getPropertyValue("background-color");
  });
  is(color, "rgb(0, 128, 0)",
     "Content is still modified from click in viewport");

  await closeRDM(tab);

  // Check color back in the browser tab
  color = await ContentTask.spawn(browser, {}, async function () {
    // eslint-disable-next-line mozilla/no-cpows-in-tests
    return content.getComputedStyle(content.document.body)
                  .getPropertyValue("background-color");
  });
  is(color, "rgb(0, 128, 0)",
     "Content is still modified from click in browser tab");

  // Check session history state
  history = await getSessionHistory(browser);
  is(history.index - 1, 1, "At page 1 in history");
  is(history.entries.length, 3, "3 pages in history");
  is(history.entries[0].url, DUMMY_1_URL, "Page 0 URL matches");
  is(history.entries[1].url, TEST_URL, "Page 1 URL matches");
  is(history.entries[2].url, DUMMY_2_URL, "Page 2 URL matches");

  await removeTab(tab);
});
