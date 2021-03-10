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
      // Disable onboarding results for general tests. They are enabled in tests
      // that specifically address onboarding.
      ["browser.urlbar.tabToSearch.onboard.interactionsLeft", 0],
    ],
  });

  await SearchTestUtils.installSearchExtension({
    name: TEST_ENGINE_NAME,
    search_url: `https://${TEST_ENGINE_DOMAIN}/`,
  });

  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits([`https://${TEST_ENGINE_DOMAIN}/`]);
  }

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

// Tests that tab-to-search results preview search mode when highlighted. These
// results are worth testing separately since they do not set the
// payload.keyword parameter.
add_task(async function basic() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
    fireInputEvent: true,
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
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
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
    fireInputEvent: true,
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
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
    fireInputEvent: true,
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
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  await SpecialPowers.popPrefEnv();
});

// Tests that we set aria-activedescendant after accessing a tab-to-search
// result with the arrow keys.
add_task(async function activedescendant_arrow() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
    fireInputEvent: true,
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
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
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
  gURLBar.focus();
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
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
});

// Test that large-style onboarding results appear and have the correct
// properties.
add_task(async function onboard() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.tabToSearch.onboard.interactionsLeft", 3]],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
    fireInputEvent: true,
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
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  UrlbarPrefs.set("tabToSearch.onboard.interactionsLeft", 3);
  delete UrlbarProviderTabToSearch.onboardingInteractionAtTime;
  await SpecialPowers.popPrefEnv();
});

// Tests that we show the onboarding result until the user interacts with it
// `browser.urlbar.tabToSearch.onboard.interactionsLeft` times.
add_task(async function onboard_limit() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.tabToSearch.onboard.interactionsLeft", 3]],
  });

  Assert.equal(
    UrlbarPrefs.get("tabToSearch.onboard.interactionsLeft"),
    3,
    "Sanity check: interactionsLeft is 3."
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
    fireInputEvent: true,
  });
  let onboardingResult = (
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
  ).result;
  Assert.equal(
    onboardingResult.payload.dynamicType,
    DYNAMIC_RESULT_TYPE,
    "The second result is an onboarding result."
  );
  await EventUtils.synthesizeKey("KEY_ArrowDown");
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    entry: "tabtosearch_onboard",
    isPreview: true,
  });
  Assert.equal(UrlbarPrefs.get("tabToSearch.onboard.interactionsLeft"), 2);
  await UrlbarTestUtils.exitSearchMode(window);

  // We don't increment the counter if we showed the onboarding result less than
  // 5 minutes ago.
  for (let i = 0; i < 5; i++) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_ENGINE_DOMAIN.slice(0, 4),
      fireInputEvent: true,
    });
    onboardingResult = (
      await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
    ).result;
    Assert.equal(
      onboardingResult.payload.dynamicType,
      DYNAMIC_RESULT_TYPE,
      "The second result is an onboarding result."
    );
    await EventUtils.synthesizeKey("KEY_ArrowDown");
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: TEST_ENGINE_NAME,
      entry: "tabtosearch_onboard",
      isPreview: true,
    });
    Assert.equal(
      UrlbarPrefs.get("tabToSearch.onboard.interactionsLeft"),
      2,
      "We shouldn't decrement interactionsLeft if an onboarding result was just shown."
    );
    await UrlbarTestUtils.exitSearchMode(window);
  }

  // If the user doesn't interact with the result, we don't increment the
  // counter.
  for (let i = 0; i < 5; i++) {
    delete UrlbarProviderTabToSearch.onboardingInteractionAtTime;
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_ENGINE_DOMAIN.slice(0, 4),
      fireInputEvent: true,
    });
    let tabToSearchResult = (
      await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
    ).result;
    Assert.equal(
      tabToSearchResult.providerName,
      "TabToSearch",
      "The second result is a tab-to-search result."
    );
    Assert.equal(
      tabToSearchResult.type,
      UrlbarUtils.RESULT_TYPE.DYNAMIC,
      "The tab-to-search result is an onboarding result."
    );
    Assert.equal(
      UrlbarPrefs.get("tabToSearch.onboard.interactionsLeft"),
      2,
      "We shouldn't decrement interactionsLeft if the user doesn't interact with onboarding."
    );
  }

  // Test that we increment the counter if the user interacts with the result
  // and it's been 5+ minutes since they last interacted with it.
  for (let i = 1; i >= 0; i--) {
    delete UrlbarProviderTabToSearch.onboardingInteractionAtTime;
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_ENGINE_DOMAIN.slice(0, 4),
      fireInputEvent: true,
    });
    let tabToSearchResult = (
      await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
    ).result;
    Assert.equal(
      tabToSearchResult.providerName,
      "TabToSearch",
      "The second result is a tab-to-search result."
    );
    Assert.equal(
      onboardingResult.payload.dynamicType,
      DYNAMIC_RESULT_TYPE,
      "The second result is an onboarding result."
    );
    await EventUtils.synthesizeKey("KEY_ArrowDown");
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: TEST_ENGINE_NAME,
      entry: "tabtosearch_onboard",
      isPreview: true,
    });
    Assert.equal(
      UrlbarPrefs.get("tabToSearch.onboard.interactionsLeft"),
      i,
      "We decremented interactionsLeft."
    );
    await UrlbarTestUtils.exitSearchMode(window);
  }

  delete UrlbarProviderTabToSearch.onboardingInteractionAtTime;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
    fireInputEvent: true,
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
    "Now that interactionsLeft is 0, we don't show onboarding results."
  );

  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  UrlbarPrefs.set("tabToSearch.onboard.interactionsLeft", 3);
  delete UrlbarProviderTabToSearch.onboardingInteractionAtTime;
  await SpecialPowers.popPrefEnv();
});

// Tests that we show at most one onboarding result at a time. See
// tests/unit/test_providerTabToSearch.js:multipleEnginesForHostname for a test
// that ensures only one normal tab-to-search result is shown in this scenario.
add_task(async function onboard_multipleEnginesForHostname() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.tabToSearch.onboard.interactionsLeft", 3]],
  });

  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: `${TEST_ENGINE_NAME}Maps`,
      search_url: `https://${TEST_ENGINE_DOMAIN}/maps/`,
    },
    true
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_ENGINE_DOMAIN.slice(0, 4),
    fireInputEvent: true,
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
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  await extension.unload();
  UrlbarPrefs.set("tabToSearch.onboard.interactionsLeft", 3);
  delete UrlbarProviderTabToSearch.onboardingInteractionAtTime;
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
  let extension = await SearchTestUtils.installSearchExtension(
    {
      id: "testunicode",
      name: engineName,
      search_url: `https://${engineDomain}/`,
    },
    true
  );
  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits([`https://${engineDomain}/`]);
  }

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: engineDomain.slice(0, 4),
    fireInputEvent: true,
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

  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  await PlacesUtils.history.clear();
  await extension.unload();
});
