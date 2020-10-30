/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests tab-to-search results. See also
 * browser/components/urlbar/tests/unit/test_providerTabToSearch.js.
 */

"use strict";

const TEST_ENGINE_NAME = "Test";
const TEST_ENGINE_DOMAIN = "example.com";

const DYNAMIC_RESULT_TYPE = "onboardTabToSearch";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProviderTabToSearch:
    "resource:///modules/UrlbarProviderTabToSearch.jsm",
});

add_task(async function setup() {
  await PlacesUtils.history.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
      ["browser.urlbar.update2.tabToComplete", true],
      // Disable onboarding results for general tests. They are enabled in tests
      // that specifically address onboarding.
      ["browser.urlbar.tabToSearch.onboard.maxShown", 0],
    ],
  });
  let testEngine = await Services.search.addEngineWithDetails(
    TEST_ENGINE_NAME,
    {
      alias: "@test",
      template: `http://${TEST_ENGINE_DOMAIN}/?search={searchTerms}`,
    }
  );
  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits([`https://${TEST_ENGINE_DOMAIN}/`]);
  }

  UrlbarProviderTabToSearch.onboardingResultCountThisSession = 0;
  await UrlbarPrefs.set("tipShownCount.tabToSearch", 0);

  registerCleanupFunction(async () => {
    UrlbarProviderTabToSearch.onboardingResultCountThisSession = 0;
    await UrlbarPrefs.set("tipShownCount.tabToSearch", 0);
    await PlacesUtils.history.clear();
    await Services.search.removeEngine(testEngine);
  });
});

// Tests that tab-to-search results preview search mode when highlighted. These
// results are worth testing separately since they do not set the
// payload.keyword parameter.
add_task(async function basic() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });
  let autofillResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(autofillResult.autofill);
  Assert.equal(
    autofillResult.url,
    `https://${TEST_ENGINE_DOMAIN}/`,
    "The autofilled URL matches the engine domain."
  );

  let tabToSearchResult = (
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
  ).result;
  Assert.equal(
    tabToSearchResult.providerName,
    "TabToSearch",
    "The second result is a tab-to-search result."
  );
  Assert.equal(
    tabToSearchResult.payload.engine,
    TEST_ENGINE_NAME,
    "The tab-to-search result is for the correct engine."
  );
  let tabToSearchDetails = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    1
  );
  let [actionTabToSearch] = await document.l10n.formatValues([
    {
      id: UrlbarUtils.WEB_ENGINE_NAMES.has(
        tabToSearchDetails.searchParams.engine
      )
        ? "urlbar-result-action-tabtosearch-web"
        : "urlbar-result-action-tabtosearch-other-engine",
      args: { engine: tabToSearchDetails.searchParams.engine },
    },
  ]);
  Assert.equal(
    tabToSearchDetails.displayed.title,
    `Search with ${tabToSearchDetails.searchParams.engine}`,
    "The result's title is set correctly."
  );
  Assert.equal(
    tabToSearchDetails.displayed.action,
    actionTabToSearch,
    "The correct action text is displayed in the tab-to-search result."
  );

  EventUtils.synthesizeKey("KEY_Tab");
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    1,
    "Sanity check: The second result is selected."
  );
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    entry: "tabtosearch",
    isPreview: true,
  });

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

// Tests that we do not set aria-activedescendant after tabbing to a
// tab-to-search result when the pref
// browser.urlbar.accessibility.tabToSearch.announceResults is true. If that
// pref is true, the result was already announced while the user was typing.
add_task(async function activedescendant_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.accessibility.tabToSearch.announceResults", true]],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "There should be two results."
  );
  let tabToSearchRow = await UrlbarTestUtils.waitForAutocompleteResultAt(
    window,
    1
  );
  Assert.equal(
    tabToSearchRow.result.providerName,
    "TabToSearch",
    "The second result is a tab-to-search result."
  );

  EventUtils.synthesizeKey("KEY_Tab");

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    entry: "tabtosearch",
    isPreview: true,
  });
  let aadID = gURLBar.inputField.getAttribute("aria-activedescendant");
  Assert.ok(!aadID, "aria-activedescendant was not set.");

  // Cycle through all the results then return to the tab-to-search result. It
  // should be announced.
  EventUtils.synthesizeKey("KEY_Tab");
  aadID = gURLBar.inputField.getAttribute("aria-activedescendant");
  let firstRow = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  Assert.equal(
    aadID,
    firstRow.id,
    "aria-activedescendant was set to the row after the tab-to-search result."
  );
  EventUtils.synthesizeKey("KEY_Tab");
  aadID = gURLBar.inputField.getAttribute("aria-activedescendant");
  Assert.equal(
    aadID,
    tabToSearchRow.id,
    "aria-activedescendant was set to the tab-to-search result."
  );

  // Now close and reopen the view, then do another search that yields a
  // tab-to-search result. aria-activedescendant should not be set when it is
  // selected.
  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });
  tabToSearchRow = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1);
  Assert.equal(
    tabToSearchRow.result.providerName,
    "TabToSearch",
    "The second result is a tab-to-search result."
  );

  EventUtils.synthesizeKey("KEY_Tab");

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    entry: "tabtosearch",
    isPreview: true,
  });
  aadID = gURLBar.inputField.getAttribute("aria-activedescendant");
  Assert.ok(!aadID, "aria-activedescendant was not set.");

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
  await SpecialPowers.popPrefEnv();
});

// Tests that we set aria-activedescendant after accessing a tab-to-search
// result with the arrow keys.
add_task(async function activedescendant_arrow() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });
  let tabToSearchRow = await UrlbarTestUtils.waitForAutocompleteResultAt(
    window,
    1
  );
  Assert.equal(
    tabToSearchRow.result.providerName,
    "TabToSearch",
    "The second result is a tab-to-search result."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    entry: "tabtosearch",
    isPreview: true,
  });
  let aadID = gURLBar.inputField.getAttribute("aria-activedescendant");
  Assert.equal(
    aadID,
    tabToSearchRow.id,
    "aria-activedescendant was set to the tab-to-search result."
  );

  // Move selection away from the tab-to-search result then return. It should
  // be announced.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  aadID = gURLBar.inputField.getAttribute("aria-activedescendant");
  Assert.equal(
    aadID,
    UrlbarTestUtils.getOneOffSearchButtons(window).selectedButton.id,
    tabToSearchRow.id,
    "aria-activedescendant was moved to the first one-off."
  );
  EventUtils.synthesizeKey("KEY_ArrowUp");
  aadID = gURLBar.inputField.getAttribute("aria-activedescendant");
  Assert.equal(
    aadID,
    tabToSearchRow.id,
    "aria-activedescendant was set to the tab-to-search result."
  );

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function tab_key_race() {
  // Mac Debug tinderboxes are just too slow and fail intermittently
  // even if the EventBufferer timeout is set to an high value.
  if (AppConstants.platform == "macosx" && AppConstants.DEBUG) {
    return;
  }
  info(
    "Test typing a letter followed shortly by Tab consistently selects a tab-to-search result"
  );
  Assert.equal(gURLBar.value, "", "Sanity check urlbar is empty");
  let promiseQueryStarted = new Promise(resolve => {
    /**
     * A no-op test provider.
     * We use this to wait for the query to start, because otherwise TAB will
     * move to the next widget since the panel is closed and there's no running
     * query. This means waiting for the UrlbarProvidersManager to at least
     * evaluate the isActive status of providers.
     * In the future we should try to reduce this latency, to defer user events
     * even more efficiently.
     */
    class ListeningTestProvider extends UrlbarProvider {
      constructor() {
        super();
      }
      get name() {
        return "ListeningTestProvider";
      }
      get type() {
        return UrlbarUtils.PROVIDER_TYPE.PROFILE;
      }
      isActive(context) {
        executeSoon(resolve);
        return false;
      }
      isRestricting(context) {
        return false;
      }
      async startQuery(context, addCallback) {
        // Nothing to do.
      }
    }
    let provider = new ListeningTestProvider();
    UrlbarProvidersManager.registerProvider(provider);
    registerCleanupFunction(async function() {
      UrlbarProvidersManager.unregisterProvider(provider);
    });
  });
  info("Type the beginning of the search string to get tab-to-search");
  EventUtils.synthesizeKey(TEST_ENGINE_DOMAIN.slice(0, 1));
  info("Awaiting for the query to start");
  await promiseQueryStarted;
  EventUtils.synthesizeKey("KEY_Tab");
  await UrlbarTestUtils.promiseSearchComplete(window);
  await TestUtils.waitForCondition(
    () => UrlbarTestUtils.getSelectedRowIndex(window) == 1,
    "Wait for tab key to be handled"
  );
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    entry: "tabtosearch",
    isPreview: true,
  });

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

// Test that large-style onboarding results appear and have the correct
// properties.
add_task(async function onboard() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.tabToSearch.onboard.maxShown", 10]],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });
  let autofillResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(autofillResult.autofill);
  Assert.equal(
    autofillResult.url,
    `https://${TEST_ENGINE_DOMAIN}/`,
    "The autofilled URL matches the engine domain."
  );
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    1,
    "Sanity check: The second result is selected."
  );

  // Now check the properties of the onboarding result.
  let onboardingElement = await UrlbarTestUtils.waitForAutocompleteResultAt(
    window,
    1
  );
  Assert.equal(
    onboardingElement.result.payload.dynamicType,
    DYNAMIC_RESULT_TYPE,
    "The tab-to-search result is an onboarding result."
  );
  Assert.equal(
    onboardingElement.result.resultSpan,
    2,
    "The correct resultSpan was set."
  );
  Assert.ok(
    onboardingElement
      .querySelector(".urlbarView-row-inner")
      .hasAttribute("selected"),
    "The onboarding element set the selected attribute."
  );

  let [
    titleOnboarding,
    actionOnboarding,
    descriptionOnboarding,
  ] = await document.l10n.formatValues([
    {
      id: "urlbar-result-action-search-w-engine",
      args: {
        engine: onboardingElement.result.payload.engine,
      },
    },
    {
      id: UrlbarUtils.WEB_ENGINE_NAMES.has(
        onboardingElement.result.payload.engine
      )
        ? "urlbar-result-action-tabtosearch-web"
        : "urlbar-result-action-tabtosearch-other-engine",
      args: { engine: onboardingElement.result.payload.engine },
    },
    {
      id: "urlbar-tabtosearch-onboard",
    },
  ]);
  let onboardingDetails = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    onboardingDetails.displayed.title,
    titleOnboarding,
    "The correct title was set."
  );
  Assert.equal(
    onboardingDetails.displayed.action,
    actionOnboarding,
    "The correct action text was set."
  );
  Assert.equal(
    onboardingDetails.element.row.querySelector(
      ".urlbarView-dynamic-onboardTabToSearch-description"
    ).textContent,
    descriptionOnboarding,
    "The correct description was set."
  );

  // Check that the onboarding result enters search mode.
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    entry: "tabtosearch_onboard",
    isPreview: true,
  });
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Enter");
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    entry: "tabtosearch_onboard",
  });
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProviderTabToSearch.onboardingResultCountThisSession = 0;
  await UrlbarPrefs.set("tipShownCount.tabToSearch", 0);
  await SpecialPowers.popPrefEnv();
});

// Tests that we show the onboarding result at most
// `browser.urlbar.tabToSearch.onboard.maxShown` times.
add_task(async function onboard_limit() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Set maxShown lower than maxShownPerSession so we hit it first.
      ["browser.urlbar.tabToSearch.onboard.maxShown", 2],
      ["browser.urlbar.tabToSearch.onboard.maxShownPerSession", 3],
    ],
  });

  let limit = UrlbarPrefs.get("tabToSearch.onboard.maxShown");
  for (let i = 0; i < limit; i++) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_ENGINE_DOMAIN.slice(0, 4),
    });
    let onboardingResult = (
      await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
    ).result;
    Assert.equal(
      onboardingResult.payload.dynamicType,
      DYNAMIC_RESULT_TYPE,
      "The second result is a tab-to-search onboarding result."
    );
    await UrlbarTestUtils.promisePopupClose(window);
  }

  // We should have exhausted our limit.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });
  let tabToSearchResult = (
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
  ).result;
  Assert.equal(
    tabToSearchResult.providerName,
    "TabToSearch",
    "The second result is a tab-to-search result."
  );
  Assert.notEqual(
    tabToSearchResult.type,
    UrlbarUtils.RESULT_TYPE.DYNAMIC,
    "The tab-to-search result is not an onboarding result."
  );

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProviderTabToSearch.onboardingResultCountThisSession = 0;
  await UrlbarPrefs.set("tipShownCount.tabToSearch", 0);
  await SpecialPowers.popPrefEnv();
});

// Tests that we show the onboarding result at most
// `browser.urlbar.tabToSearch.onboard.maxShownPerSession` times per session.
add_task(async function onboard_sessionlimit() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.tabToSearch.onboard.maxShown", 10],
      // browser.urlbar.tabToSearch.onboard.maxShownPerSession can be its
      // default value.
    ],
  });

  let sessionLimit = UrlbarPrefs.get("tabToSearch.onboard.maxShownPerSession");

  for (let i = 0; i < sessionLimit; i++) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_ENGINE_DOMAIN.slice(0, 4),
    });
    let onboardingResult = (
      await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
    ).result;
    Assert.equal(
      onboardingResult.payload.dynamicType,
      DYNAMIC_RESULT_TYPE,
      "The second result is a tab-to-search onboarding result."
    );
    await UrlbarTestUtils.promisePopupClose(window);
  }

  // We should have exhausted our per-session limit.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });
  let tabToSearchResult = (
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
  ).result;
  Assert.equal(
    tabToSearchResult.providerName,
    "TabToSearch",
    "The second result is a tab-to-search result."
  );
  Assert.notEqual(
    tabToSearchResult.type,
    UrlbarUtils.RESULT_TYPE.DYNAMIC,
    "The tab-to-search result is not an onboarding result."
  );

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProviderTabToSearch.onboardingResultCountThisSession = 0;
  await UrlbarPrefs.set("tipShownCount.tabToSearch", 0);
  await SpecialPowers.popPrefEnv();
});

// Tests that we show at most one onboarding result at a time. See
// tests/unit/test_providerTabToSearch.js:multipleEnginesForHostname for a test
// that ensures only one normal tab-to-search result is shown in this scenario.
add_task(async function onboard_multipleEnginesForHostname() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.tabToSearch.onboard.maxShown", 10]],
  });

  let testEngineMaps = await Services.search.addEngineWithDetails(
    `${TEST_ENGINE_NAME}Maps`,
    {
      template: `http://${TEST_ENGINE_DOMAIN}/maps/?search={searchTerms}`,
    }
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "Only two results are shown."
  );
  let firstResult = (
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0)
  ).result;
  Assert.notEqual(
    firstResult.providerName,
    "TabToSearch",
    "The first result is not from TabToSearch."
  );
  let secondResult = (
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
  ).result;
  Assert.equal(
    secondResult.providerName,
    "TabToSearch",
    "The second result is from TabToSearch."
  );
  Assert.equal(
    secondResult.type,
    UrlbarUtils.RESULT_TYPE.DYNAMIC,
    "The tab-to-search result is the only onboarding result."
  );
  await Services.search.removeEngine(testEngineMaps);
  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProviderTabToSearch.onboardingResultCountThisSession = 0;
  await UrlbarPrefs.set("tipShownCount.tabToSearch", 0);
  await SpecialPowers.popPrefEnv();
});

// Tests that we stop showing onboarding results after one is interacted with if
// the tabToSearch.onboard.oneInteraction pref is set to true.
add_task(async function onboard_oneInteraction_true() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.tabToSearch.onboard.maxShown", 10],
      ["browser.urlbar.tabToSearch.onboard.oneInteraction", true],
    ],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });

  EventUtils.synthesizeKey("KEY_Tab");
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    entry: "tabtosearch_onboard",
    isPreview: true,
  });
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Enter");
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    // Implicit check that we selected an onboarding result.
    entry: "tabtosearch_onboard",
  });
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProviderTabToSearch.onboardingResultCountThisSession = 0;

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });

  let result = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
    .result;
  Assert.equal(
    result.providerName,
    "TabToSearch",
    "The second result is a tab-to-search result."
  );
  Assert.notEqual(
    result.type,
    UrlbarUtils.RESULT_TYPE.DYNAMIC,
    "The tab-to-search result was not an onboarding result."
  );

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProviderTabToSearch.onboardingResultCountThisSession = 0;
  await UrlbarPrefs.set("tipShownCount.tabToSearch", 0);
  await SpecialPowers.popPrefEnv();
});

// Tests that we continue showing onboarding results after one is interacted
// with if the tabToSearch.onboard.oneInteraction pref is set to false.
add_task(async function onboard_oneInteraction_false() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.tabToSearch.onboard.maxShown", 10],
      ["browser.urlbar.tabToSearch.onboard.oneInteraction", false],
    ],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });

  EventUtils.synthesizeKey("KEY_Tab");
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    entry: "tabtosearch_onboard",
    isPreview: true,
  });
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Enter");
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    // Implicit check that we selected an onboarding result.
    entry: "tabtosearch_onboard",
  });
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
  });

  let result = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
    .result;
  Assert.equal(
    result.providerName,
    "TabToSearch",
    "The second result is a tab-to-search result."
  );
  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.DYNAMIC,
    "The tab-to-search result is an onboarding result."
  );

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProviderTabToSearch.onboardingResultCountThisSession = 0;
  await UrlbarPrefs.set("tipShownCount.tabToSearch", 0);
  await SpecialPowers.popPrefEnv();
});

// Tests that engines with names containing extended Unicode characters can be
// recognized as general-web engines and that their tab-to-search results
// display the correct string.
add_task(async function extended_unicode_in_engine() {
  // Baidu's localized name. We expect this tab-to-search result shows the
  // general-web engine string because Baidu is included in WEB_ENGINE_NAMES.
  let engineName = "百度";
  let engineDomain = "example-2.com";
  let testEngine = await Services.search.addEngineWithDetails(engineName, {
    template: `http://${engineDomain}/?search={searchTerms}`,
  });
  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits([`https://${engineDomain}/`]);
  }

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: engineDomain.slice(0, 4),
  });
  let tabToSearchDetails = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    1
  );
  Assert.equal(
    tabToSearchDetails.searchParams.engine,
    engineName,
    "The tab-to-search engine name contains extended Unicode characters."
  );
  let [actionTabToSearch] = await document.l10n.formatValues([
    {
      id: "urlbar-result-action-tabtosearch-web",
      args: { engine: tabToSearchDetails.searchParams.engine },
    },
  ]);
  Assert.equal(
    tabToSearchDetails.displayed.action,
    actionTabToSearch,
    "The correct action text is displayed in the tab-to-search result."
  );

  await UrlbarTestUtils.promisePopupClose(window);
  await PlacesUtils.history.clear();
  await Services.search.removeEngine(testEngine);
});
