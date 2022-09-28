/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests QuickActions.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.sys.mjs",
});

const DUMMY_PAGE =
  "http://example.com/browser/browser/base/content/test/general/dummy_page.html";

let testActionCalled = 0;

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quickactions", true],
      ["browser.urlbar.shortcuts.quickactions", true],
      ["screenshots.browser.component.enabled", true],
      ["extensions.screenshots.disabled", false],
    ],
  });

  UrlbarProviderQuickActions.addAction("testaction", {
    commands: ["testaction"],
    label: "quickactions-downloads",
    onPick: () => testActionCalled++,
  });

  registerCleanupFunction(() => {
    UrlbarProviderQuickActions.removeAction("testaction");
  });
});

add_task(async function basic() {
  info("The action isnt shown when not matched");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "nomatch",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "We did no match anything"
  );

  info("A prefix of the command matches");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "testact",
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "We matched the action"
  );

  info("The callback of the action is fired when selected");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
  EventUtils.synthesizeKey("KEY_Enter", {}, window);
  Assert.equal(testActionCalled, 1, "Test actionwas called");
});

add_task(async function enter_search_mode_button() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  let oneOffButton = await TestUtils.waitForCondition(() =>
    window.document.getElementById("urlbar-engine-one-off-item-actions")
  );
  Assert.ok(oneOffButton, "One off button is available when preffed on");
  EventUtils.synthesizeMouseAtCenter(oneOffButton, {}, window);

  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
    entry: "oneoff",
  });

  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  Assert.ok(true, "Actions are shown when we enter actions search mode.");

  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
});

add_task(async function enter_search_mode_key() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "> ",
  });
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
    entry: "typed",
  });
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
});

add_task(async function test_disabled() {
  testActionCalled = 0;
  UrlbarProviderQuickActions.addAction("disabledaction", {
    commands: ["disabledaction"],
    isActive: () => false,
    label: "quickactions-restart",
    onPick: () => testActionCalled++,
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "disabled",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "The action is displayed"
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.ok(
    UrlbarTestUtils.getSelectedElement(window).classList.contains(
      "urlbarView-button-help"
    ),
    "The selected element should be the onboarding button."
  );

  let disabledButton = window.document.querySelector(
    ".urlbarView-row[dynamicType=quickactions]"
  );
  EventUtils.synthesizeMouseAtCenter(disabledButton, {});
  Assert.equal(
    testActionCalled,
    0,
    "onPick for disabled action was not called"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
  UrlbarProviderQuickActions.removeAction("disabledaction");
});

add_task(async function test_screenshot_disabled() {
  let onLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "about:blank"
  );
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:blank");
  await onLoaded;

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "screenshot",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "The action is displayed"
  );
  let screenshotButton = window.document.querySelector(
    ".urlbarView-row[dynamicType=quickactions] .urlbarView-quickaction-row"
  );
  Assert.equal(
    screenshotButton.getAttribute("disabled"),
    "disabled",
    "Screenshot button is disabled on about pages"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
});

add_task(async function match_in_phrase() {
  UrlbarProviderQuickActions.addAction("newtestaction", {
    commands: ["matchingstring"],
    label: "quickactions-downloads",
  });

  info("The action is matched when at end of input");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "Test we match at end of matchingstring",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "We matched the action"
  );
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
  UrlbarProviderQuickActions.removeAction("newtestaction");
});

async function isScreenshotInitialized() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let screenshotsChild = content.windowGlobalChild.getActor(
      "ScreenshotsComponent"
    );
    return screenshotsChild?._overlay?._initialized;
  });
}

add_task(async function test_screenshot() {
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, DUMMY_PAGE);
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
    "Screenshot component is active"
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

add_task(async function test_other_search_mode() {
  let defaultEngine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "searchSuggestionEngine.xml"
  );
  defaultEngine.alias = "testalias";
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(
    defaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: defaultEngine.alias + " ",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    0,
    "The results should be empty as no actions are displayed in other search modes"
  );
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: defaultEngine.name,
    entry: "typed",
  });
  Services.search.setDefault(
    oldDefaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
});
