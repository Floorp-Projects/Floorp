/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.resultMenu", true]],
  });
});

async function openResultMenuAndPressAccesskey(resultIndex, accesskey) {
  let menuButton = UrlbarTestUtils.getButtonForResultIndex(
    window,
    "menu",
    resultIndex
  );
  ok(menuButton, `found the menu button at result index ${resultIndex}`);
  while (gURLBar.view.selectedRowIndex != resultIndex) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }
  EventUtils.synthesizeKey("KEY_Tab");
  is(
    UrlbarTestUtils.getSelectedElement(window),
    menuButton,
    `selected the menu button at result index ${resultIndex}`
  );

  let promiseMenuOpen = BrowserTestUtils.waitForEvent(
    gURLBar.view.resultMenu,
    "popupshown"
  );
  EventUtils.synthesizeKey("KEY_Enter", {});
  info("waiting for the menu to open");
  await promiseMenuOpen;

  info(`pressing access key (${accesskey}) to activate menu item`);
  let promiseCommand = BrowserTestUtils.waitForEvent(
    gURLBar.view.resultMenu,
    "command"
  );
  EventUtils.synthesizeKey(accesskey);
  info("waiting for command event");
  await promiseCommand;
}

add_task(async function test_remove_history() {
  const TEST_URL = "https://remove.me/from_urlbar/";
  await PlacesTestUtils.addVisits(TEST_URL);

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });

  let promiseVisitRemoved = PlacesTestUtils.waitForNotification(
    "page-removed",
    events => events[0].url === TEST_URL,
    "places"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "from_urlbar",
  });

  const resultIndex = 1;
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(result.url, TEST_URL, "Found the expected result");

  let expectedResultCount = UrlbarTestUtils.getResultCount(window) - 1;

  await openResultMenuAndPressAccesskey(resultIndex, "R");

  const removeEvents = await promiseVisitRemoved;
  Assert.ok(
    removeEvents[0].isRemovedFromStore,
    "isRemovedFromStore should be true"
  );

  await TestUtils.waitForCondition(
    () => UrlbarTestUtils.getResultCount(window) == expectedResultCount,
    "Waiting for the result to disappear"
  );

  for (let i = 0; i < expectedResultCount; i++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.notEqual(
      details.url,
      TEST_URL,
      "Should not find the test URL in the remaining results"
    );
  }

  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function test_remove_search_history() {
  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });
  let engine = Services.search.getEngineByName("Example");
  await Services.search.moveEngine(engine, 0);
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.maxHistoricalSearchSuggestions", 1],
    ],
  });

  let formHistoryValue = "foobar";
  await UrlbarTestUtils.formHistory.add([formHistoryValue]);

  let formHistory = (
    await UrlbarTestUtils.formHistory.search({
      value: formHistoryValue,
    })
  ).map(entry => entry.value);
  Assert.deepEqual(
    formHistory,
    [formHistoryValue],
    "Should find form history after adding it"
  );

  let promiseRemoved = UrlbarTestUtils.formHistory.promiseChanged("remove");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });

  let index = 1;
  let count = UrlbarTestUtils.getResultCount(window);
  for (; index < count; index++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.source == UrlbarUtils.RESULT_SOURCE.HISTORY
    ) {
      break;
    }
  }
  Assert.ok(index < count, "Result found");

  await openResultMenuAndPressAccesskey(index, "R");
  await promiseRemoved;

  await TestUtils.waitForCondition(
    () => UrlbarTestUtils.getResultCount(window) == count - 1,
    "Waiting for the result to disappear"
  );

  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.ok(
      result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
        result.source != UrlbarUtils.RESULT_SOURCE.HISTORY,
      "Should not find the form history result in the remaining results"
    );
  }

  await UrlbarTestUtils.promisePopupClose(window);

  formHistory = (
    await UrlbarTestUtils.formHistory.search({
      value: formHistoryValue,
    })
  ).map(entry => entry.value);
  Assert.deepEqual(
    formHistory,
    [],
    "Should not find form history after removing it"
  );

  await SpecialPowers.popPrefEnv();
});

add_task(async function firefoxSuggest() {
  const url = "https://example.com/hey-there";
  const helpUrl = "https://example.com/help";
  let provider = new UrlbarTestUtils.TestProvider({
    priority: Infinity,
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          url,
          isBlockable: true,
          blockL10n: { id: "urlbar-result-menu-dismiss-firefox-suggest" },
          helpUrl,
          helpL10n: {
            id: "urlbar-result-menu-learn-more-about-firefox-suggest",
          },
        }
      ),
    ],
  });

  // Implement the provider's `blockResult()`. Return true from it so the view
  // removes the row after it's called.
  let blockResultCallCount = 0;
  provider.blockResult = () => {
    blockResultCallCount++;
    return true;
  };

  UrlbarProvidersManager.registerProvider(provider);

  async function openResults() {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });

    Assert.equal(
      UrlbarTestUtils.getResultCount(window),
      1,
      "There should be one result"
    );

    let row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
    Assert.equal(
      row.result.payload.url,
      url,
      "The result should be in the first row"
    );
  }

  await openResults();
  let tabOpenPromise = BrowserTestUtils.waitForNewTab(gBrowser, helpUrl);
  await openResultMenuAndPressAccesskey(0, "L");
  info("Waiting for help URL to load in a new tab");
  await tabOpenPromise;
  gBrowser.removeCurrentTab();

  await openResults();
  await openResultMenuAndPressAccesskey(0, "D");

  Assert.equal(
    blockResultCallCount,
    1,
    "blockResult() should have been called once"
  );
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    0,
    "There should be no results after blocking"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarProvidersManager.unregisterProvider(provider);
});
