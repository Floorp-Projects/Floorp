/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "http://mochi.test:8888/browser/browser/base/content/test/general/file_favicon_change.html";

add_task(async function() {
  let extraTab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  extraTab.linkedBrowser.loadURI(TEST_URL);
  let tabLoaded = BrowserTestUtils.browserLoaded(extraTab.linkedBrowser);
  let expectedFavicon = "http://example.org/one-icon";
  let haveChanged = PromiseUtils.defer();
  let observer = new MutationObserver(function(mutations) {
    for (let mut of mutations) {
      if (mut.attributeName != "image") {
        continue;
      }
      let imageVal = extraTab.getAttribute("image").replace(/#.*$/, "");
      // Ignore chrome favicons set on the tab before the actual page load.
      if (!imageVal || !imageVal.startsWith("http://example.org/")) {
        // The value gets removed because it doesn't load.
        continue;
      }
      is(imageVal, expectedFavicon, "Favicon image should correspond to expected image.");
      haveChanged.resolve();
    }
  });
  observer.observe(extraTab, {attributes: true});
  await tabLoaded;
  await haveChanged.promise;
  haveChanged = PromiseUtils.defer();
  expectedFavicon = "http://example.org/other-icon";
  ContentTask.spawn(extraTab.linkedBrowser, null, function() {
    let ev = new content.CustomEvent("PleaseChangeFavicon", {});
    content.dispatchEvent(ev);
  });
  await haveChanged.promise;
  observer.disconnect();
  gBrowser.removeTab(extraTab);
});

