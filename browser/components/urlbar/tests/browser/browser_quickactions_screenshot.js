/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * QuickActions tests that touch screenshot functionality.
 */

"use strict";

requestLongerTimeout(3);

const DUMMY_PAGE =
  "https://example.com/browser/browser/base/content/test/general/dummy_page.html";

async function isScreenshotInitialized() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let screenshotsChild = content.windowGlobalChild.getActor(
      "ScreenshotsComponent"
    );
    return screenshotsChild?.overlay?.initialized;
  });
}

async function clickQuickActionOneoffButton() {
  const oneOffButton = await TestUtils.waitForCondition(() =>
    window.document.getElementById("urlbar-engine-one-off-item-actions")
  );
  Assert.ok(oneOffButton, "One off button is available when preffed on");

  EventUtils.synthesizeMouseAtCenter(oneOffButton, {}, window);
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
    entry: "oneoff",
  });
}

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quickactions.enabled", true],
      ["browser.urlbar.suggest.quickactions", true],
      ["browser.urlbar.shortcuts.quickactions", true],
    ],
  });
});

add_task(async function test_screenshot() {
  await SpecialPowers.pushPrefEnv({
    set: [["screenshots.browser.component.enabled", true]],
  });

  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, DUMMY_PAGE);
  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    DUMMY_PAGE
  );

  info("The action is matched when at end of input");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "screenshot",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "We matched the action"
  );
  let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.DYNAMIC);
  Assert.equal(result.providerName, "quickactions");

  info("Trigger the screenshot mode");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
  EventUtils.synthesizeKey("KEY_Enter", {}, window);
  await TestUtils.waitForCondition(
    isScreenshotInitialized,
    "Screenshot component is active",
    200,
    100
  );

  info("Press Escape to exit screenshot mode");
  EventUtils.synthesizeKey("KEY_Escape", {}, window);
  await TestUtils.waitForCondition(
    async () => !(await isScreenshotInitialized()),
    "Screenshot component has been dismissed"
  );

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
});

add_task(async function search_mode_on_webpage() {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );

  info("Show result by click");
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {}, window);
  });
  await UrlbarTestUtils.promiseSearchComplete(window);

  info("Enter quick action search mode");
  await clickQuickActionOneoffButton();
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  Assert.ok(true, "Actions are shown when we enter actions search mode.");

  info("Trigger the screenshot mode");
  const initialActionButtons = window.document.querySelectorAll(
    ".urlbarView-row[dynamicType=quickactions] .urlbarView-quickaction-button"
  );
  let screenshotButton;
  for (let i = 0; i < initialActionButtons.length; i++) {
    const item = initialActionButtons.item(i);
    if (item.dataset.key === "screenshot") {
      screenshotButton = item;
      break;
    }
  }
  EventUtils.synthesizeMouseAtCenter(screenshotButton, {}, window);
  await TestUtils.waitForCondition(
    isScreenshotInitialized,
    "Screenshot component is active",
    200,
    100
  );

  info("Press Escape to exit screenshot mode");
  EventUtils.synthesizeKey("KEY_Escape", {}, window);
  await TestUtils.waitForCondition(
    async () => !(await isScreenshotInitialized()),
    "Screenshot component has been dismissed"
  );

  info("Check the urlbar state");
  Assert.equal(gURLBar.value, UrlbarTestUtils.trimURL("https://example.com"));
  Assert.equal(gURLBar.getAttribute("pageproxystate"), "valid");

  info("Show result again");
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {}, window);
  });
  await UrlbarTestUtils.promiseSearchComplete(window);

  info("Enter quick action search mode again");
  await clickQuickActionOneoffButton();
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  const finalActionButtons = window.document.querySelectorAll(
    ".urlbarView-row[dynamicType=quickactions] .urlbarView-quickaction-button"
  );

  info("Check the action buttons and the urlbar");
  Assert.equal(
    finalActionButtons.length,
    initialActionButtons.length,
    "The same buttons as initially displayed will display"
  );
  Assert.equal(gURLBar.value, "");

  info("Clean up");
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
  BrowserTestUtils.removeTab(tab);
  await PlacesUtils.history.clear();
});
