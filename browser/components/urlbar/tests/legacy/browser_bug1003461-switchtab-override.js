/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_PATH = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content", "http://example.org/");
const TEST_URL = `${TEST_PATH}dummy_page.html`;


add_task(async function test_switchtab_override() {
  info("Opening first tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Opening and selecting second tab");
  let secondTab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  registerCleanupFunction(() => {
    try {
      gBrowser.removeTab(tab);
      gBrowser.removeTab(secondTab);
    } catch (ex) { /* tabs may have already been closed in case of failure */ }
  });

  info("Wait for autocomplete");
  let deferred = PromiseUtils.defer();
  let onSearchComplete = gURLBar.onSearchComplete;
  registerCleanupFunction(() => {
    gURLBar.onSearchComplete = onSearchComplete;
  });
  gURLBar.onSearchComplete = function() {
    ok(gURLBar.popupOpen, "The autocomplete popup is correctly open");
    onSearchComplete.apply(gURLBar);
    deferred.resolve();
  };

  gURLBar.focus();
  gURLBar.value = "dummy_pag";
  EventUtils.sendString("e");
  await deferred.promise;

  info("Select second autocomplete popup entry");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  ok(/moz-action:switchtab/.test(gURLBar.value), "switch to tab entry found");

  info("Override switch-to-tab");
  deferred = PromiseUtils.defer();
  // In case of failure this would switch tab.
  let onTabSelect = event => {
    deferred.reject(new Error("Should have overridden switch to tab"));
  };
  gBrowser.tabContainer.addEventListener("TabSelect", onTabSelect);
  registerCleanupFunction(() => {
    gBrowser.tabContainer.removeEventListener("TabSelect", onTabSelect);
  });
  // Otherwise it would load the page.
  BrowserTestUtils.browserLoaded(secondTab.linkedBrowser).then(deferred.resolve);

  EventUtils.synthesizeKey("KEY_Shift", {type: "keydown"});
  EventUtils.synthesizeKey("KEY_Enter");
  info(`gURLBar.value = ${gURLBar.value}`);
  EventUtils.synthesizeKey("KEY_Shift", {type: "keyup"});
  await deferred.promise;

  await PlacesUtils.history.clear();
});
