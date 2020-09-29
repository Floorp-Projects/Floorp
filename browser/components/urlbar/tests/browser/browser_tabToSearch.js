/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests tab-to-search results. See also
 * browser/components/urlbar/tests/unit/test_providerTabToSearch.js.
 */

"use strict";

const TEST_ENGINE_NAME = "Test";
const TEST_ENGINE_DOMAIN = "example.com";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
      ["browser.urlbar.update2.tabToComplete", true],
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

  registerCleanupFunction(async () => {
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
