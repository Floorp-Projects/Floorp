/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test viewports basics after opening, like size and location

const TEST_URL = "http://example.org/";

addRDMTask(TEST_URL, function*({ ui }) {
  let store = ui.toolWindow.store;

  // Wait until the viewport has been added
  yield waitUntilState(store, state => state.viewports.length == 1);

  // A single viewport of default size appeared
  let browser = ui.toolWindow.document.querySelector(".browser");
  is(browser.width, "320", "Viewport has default width");
  is(browser.height, "480", "Viewport has default height");

  // Browser's location should match original tab
  // TODO: For the moment, we have parent process <iframe>s and we can just
  // check the location directly.  Bug 1240896 will change this to <iframe
  // mozbrowser remote>, which is in the child process, so ContentTask or
  // similar will be needed.
  yield waitForFrameLoad(browser, TEST_URL);
  is(browser.contentWindow.location.href, TEST_URL,
     "Viewport location matches");
});
