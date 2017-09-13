"use strict";

const TEST_URL = "http://mochi.test:8888/browser/browser/base/content/test/general/file_favicon_change_not_in_document.html"

add_task(async function() {
  let extraTab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let tabLoaded = promiseTabLoaded(extraTab);
  extraTab.linkedBrowser.loadURI(TEST_URL);
  let expectedFavicon = "http://example.org/one-icon";
  let haveChanged = PromiseUtils.defer();
  let observer = new MutationObserver(function(mutations) {
    for (let mut of mutations) {
      if (mut.attributeName != "image") {
        continue;
      }
      let imageVal = extraTab.getAttribute("image").replace(/#.*$/, "");
      if (!imageVal) {
        // The value gets removed because it doesn't load.
        continue;
      }
      is(imageVal, expectedFavicon, "Favicon image should correspond to expected image.");
      haveChanged.resolve();
    }
  });
  observer.observe(extraTab, {attributes: true});
  await tabLoaded;
  expectedFavicon = "http://example.org/yet-another-icon";
  haveChanged = PromiseUtils.defer();
  await haveChanged.promise;
  observer.disconnect();
  gBrowser.removeTab(extraTab);
});


