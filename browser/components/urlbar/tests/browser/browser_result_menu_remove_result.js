/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.resultMenu", true]],
  });
});

async function openResultMenuAndPressAccesskey(resultIndex, accesskey) {
  let promiseMenuOpen = BrowserTestUtils.waitForEvent(
    gURLBar.view.resultMenu,
    "popupshown"
  );
  let promiseMenuClosed = BrowserTestUtils.waitForEvent(
    gURLBar.view.resultMenu,
    "popuphidden"
  );
  let menuButton = UrlbarTestUtils.getButtonForResultIndex(
    window,
    "menu",
    resultIndex
  );
  ok(menuButton, `found the menu button at result index ${resultIndex}`);
  await EventUtils.synthesizeMouseAtCenter(menuButton, {}, window);
  info("waiting for the menu to open");
  await promiseMenuOpen;
  info(
    `pressing access key (${accesskey}) of the menu item to remove the result`
  );
  EventUtils.synthesizeKey(accesskey);
  info("waiting for the menu to close");
  await promiseMenuClosed;
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
