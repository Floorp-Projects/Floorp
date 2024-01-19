/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This test ensures that overriding switch-to-tab correctly loads the page
 * rather than switching to it.
 */

"use strict";

const TEST_URL = `${TEST_BASE_URL}dummy_page.html`;

add_task(async function test_switchtab_override() {
  info("Opening first tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Opening and selecting second tab");
  let secondTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  registerCleanupFunction(() => {
    try {
      gBrowser.removeTab(tab);
      gBrowser.removeTab(secondTab);
    } catch (ex) {
      /* tabs may have already been closed in case of failure */
    }
  });

  info("Wait for autocomplete");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "dummy_page",
  });

  info("Select second autocomplete popup entry");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    UrlbarTestUtils.getSelectedRowIndex(window)
  );
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.TAB_SWITCH);

  // Check to see if the switchtab label is visible and
  // all other labels are hidden
  const allLabels = document.getElementById("urlbar-label-box").children;
  for (let label of allLabels) {
    if (label.id == "urlbar-label-switchtab") {
      Assert.ok(BrowserTestUtils.isVisible(label));
    } else {
      Assert.ok(BrowserTestUtils.isHidden(label));
    }
  }

  info("Override switch-to-tab");
  let deferred = Promise.withResolvers();
  // In case of failure this would switch tab.
  let onTabSelect = event => {
    deferred.reject(new Error("Should have overridden switch to tab"));
  };
  gBrowser.tabContainer.addEventListener("TabSelect", onTabSelect);
  registerCleanupFunction(() => {
    gBrowser.tabContainer.removeEventListener("TabSelect", onTabSelect);
  });
  // Otherwise it would load the page.
  BrowserTestUtils.browserLoaded(secondTab.linkedBrowser).then(
    deferred.resolve
  );

  EventUtils.synthesizeKey("KEY_Shift", { type: "keydown" });

  // Checks that all labels are hidden when Shift is held down on the SwitchToTab result
  for (let label of allLabels) {
    Assert.ok(BrowserTestUtils.isHidden(label));
  }

  registerCleanupFunction(() => {
    // Avoid confusing next tests by leaving a pending keydown.
    EventUtils.synthesizeKey("KEY_Shift", { type: "keyup" });
  });

  let attribute = "action-override";
  Assert.ok(
    gURLBar.view.panel.hasAttribute(attribute),
    "We should be overriding"
  );

  EventUtils.synthesizeKey("KEY_Enter");
  info(`gURLBar.value = ${gURLBar.value}`);
  await deferred.promise;

  // Blurring the urlbar should have cleared the override.
  Assert.ok(
    !gURLBar.view.panel.hasAttribute(attribute),
    "We should not be overriding anymore"
  );

  await PlacesUtils.history.clear();
  gBrowser.removeTab(tab);
  gBrowser.removeTab(secondTab);
});
