/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function* test_switchtab_override() {
  let testURL = "http://example.org/browser/browser/base/content/test/urlbar/dummy_page.html";

  info("Opening first tab");
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, testURL);

  info("Opening and selecting second tab");
  let secondTab = gBrowser.selectedTab = gBrowser.addTab();
  registerCleanupFunction(() => {
    try {
      gBrowser.removeTab(tab);
      gBrowser.removeTab(secondTab);
    } catch (ex) { /* tabs may have already been closed in case of failure */ }
  });

  info("Wait for autocomplete")
  let deferred = Promise.defer();
  let onSearchComplete = gURLBar.onSearchComplete;
  registerCleanupFunction(() => {
    gURLBar.onSearchComplete = onSearchComplete;
  });
  gURLBar.onSearchComplete = function () {
    ok(gURLBar.popupOpen, "The autocomplete popup is correctly open");
    onSearchComplete.apply(gURLBar);
    deferred.resolve();
  }

  gURLBar.focus();
  gURLBar.value = "dummy_pag";
  EventUtils.synthesizeKey("e", {});
  yield deferred.promise;

  info("Select second autocomplete popup entry");
  EventUtils.synthesizeKey("VK_DOWN", {});
  ok(/moz-action:switchtab/.test(gURLBar.value), "switch to tab entry found");

  info("Override switch-to-tab");
  deferred = Promise.defer();
  // In case of failure this would switch tab.
  let onTabSelect = event => {
    deferred.reject(new Error("Should have overridden switch to tab"));
  };
  gBrowser.tabContainer.addEventListener("TabSelect", onTabSelect, false);
  registerCleanupFunction(() => {
    gBrowser.tabContainer.removeEventListener("TabSelect", onTabSelect, false);
  });
  // Otherwise it would load the page.
  BrowserTestUtils.browserLoaded(secondTab.linkedBrowser).then(deferred.resolve);

  EventUtils.synthesizeKey("VK_SHIFT", { type: "keydown" });
  EventUtils.synthesizeKey("VK_RETURN", { });
  info(`gURLBar.value = ${gURLBar.value}`);
  EventUtils.synthesizeKey("VK_SHIFT", { type: "keyup" });
  yield deferred.promise;

  yield PlacesTestUtils.clearHistory();
});
