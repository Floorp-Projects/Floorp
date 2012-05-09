/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/browser/browser/components/thumbnails/" +
            "test/background_red_redirect.sjs";

/**
 * These tests ensure that we save and provide thumbnails for redirecting sites.
 */
function runTests() {
  // Kick off history by loading a tab first or the test fails in single mode.
  yield addTab(URL);
  gBrowser.removeTab(gBrowser.selectedTab);

  // Create a tab, redirecting to a page with a red background.
  yield addTab(URL);
  yield captureAndCheckColor(255, 0, 0, "we have a red thumbnail");

  // Wait until the referrer's thumbnail's file has been written.
  yield whenFileExists(URL);
  yield checkThumbnailColor(URL, 255, 0, 0, "referrer has a red thumbnail");
}

function whenFileExists(aURL) {
  let callback = next;

  let file = PageThumbsStorage.getFileForURL(aURL);
  if (!file.exists())
    callback = function () whenFileExists(aURL);

  executeSoon(callback);
}
