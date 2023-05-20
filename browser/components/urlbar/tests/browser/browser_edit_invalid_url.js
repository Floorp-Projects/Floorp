/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Checks that we trim invalid urls when they are selected, so that if the user
// modifies the selected url, or just closes the results pane, we do a visit
// rather than searching for the trimmed string.

const url = BrowserUIUtils.trimURLProtocol + "invalid.somehost/mytest";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.trimURLs", true]],
  });
  await PlacesTestUtils.addVisits(url);
  registerCleanupFunction(PlacesUtils.history.clear);
});

add_task(async function test_escape() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "invalid",
  });
  // Look for our result.
  let resultCount = UrlbarTestUtils.getResultCount(window);
  Assert.greater(resultCount, 1, "There should be at least two results");
  for (let i = 0; i < resultCount; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    info(`Result at ${i} has url ${result.url}`);
    if (result.url.startsWith(url)) {
      break;
    }
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }
  Assert.equal(
    gURLBar.value,
    url,
    "The string displayed in the textbox should be the untrimmed url"
  );
  // Close the results pane by ESC.
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
  // Confirm the result and check the loaded page.
  let promise = waitforLoadURL();
  EventUtils.synthesizeKey("KEY_Enter");
  let loadedUrl = await promise;
  Assert.equal(loadedUrl, url, "Should try to load a url");
});

add_task(async function test_edit_url() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "invalid",
  });
  // Look for our result.
  let resultCount = UrlbarTestUtils.getResultCount(window);
  Assert.greater(resultCount, 1, "There should be at least two results");
  for (let i = 1; i < resultCount; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    info(`Result at ${i} has url ${result.url}`);
    if (result.url.startsWith(url)) {
      break;
    }
  }
  Assert.equal(
    gURLBar.value,
    url,
    "The string displayed in the textbox should be the untrimmed url"
  );
  // Modify the url.
  EventUtils.synthesizeKey("2");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL, "Should visit a url");
  Assert.equal(result.url, url + "2", "Should visit the modified url");

  // Confirm the result and check the loaded page.
  let promise = waitforLoadURL();
  EventUtils.synthesizeKey("KEY_Enter");
  let loadedUrl = await promise;
  Assert.equal(loadedUrl, url + "2", "Should try to load the modified url");
});

async function waitforLoadURL() {
  let sandbox = sinon.createSandbox();
  let loadedUrl = await new Promise(resolve =>
    sandbox.stub(gURLBar, "_loadURL").callsFake(resolve)
  );
  sandbox.restore();
  return loadedUrl;
}
