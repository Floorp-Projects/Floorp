/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests selecting a result, and editing the value of that autocompleted result.
 */

add_task(async function () {
  SpecialPowers.pushPrefEnv({ set: [["browser.urlbar.trimHttps", false]] });
  await PlacesUtils.history.clear();

  await PlacesTestUtils.addVisits([
    { uri: makeURI("http://example.com/foo") },
    { uri: makeURI("http://example.com/foo/bar") },
  ]);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  registerCleanupFunction(async function () {
    BrowserTestUtils.removeTab(tab);
    await PlacesUtils.history.clear();
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "http://example.com",
  });

  const initialIndex = UrlbarTestUtils.getSelectedRowIndex(window);

  info("Key Down to select the next item.");
  EventUtils.synthesizeKey("KEY_ArrowDown");

  let nextIndex = initialIndex + 1;
  let nextResult = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    nextIndex
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    nextIndex,
    "Should have selected the next item"
  );
  Assert.equal(
    gURLBar.untrimmedValue,
    nextResult.url,
    "Should have completed the URL"
  );

  info("Press backspace");
  EventUtils.synthesizeKey("KEY_Backspace");
  await UrlbarTestUtils.promiseSearchComplete(window);

  let editedValue = gURLBar.value;
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    initialIndex,
    "Should have selected the initialIndex again"
  );
  Assert.notEqual(editedValue, nextResult.url, "The URL has changed.");

  let docLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
    "http://" + editedValue,
    gBrowser.selectedBrowser
  );

  info("Press return to load edited URL.");

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Enter");
  });

  await docLoad;
});
