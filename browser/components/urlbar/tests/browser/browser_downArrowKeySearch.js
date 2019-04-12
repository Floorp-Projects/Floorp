/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Checks that pressing the down arrow key starts the proper searches, depending
// on the input value/state.

"use strict";

add_task(async function init() {
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits("http://example.com/");
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });
});

add_task(async function url() {
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    gURLBar.focus();
    gURLBar.selectionEnd = gURLBar.value.length;
    gURLBar.selectionStart = gURLBar.value.length;
    EventUtils.synthesizeKey("KEY_ArrowDown");
    await UrlbarTestUtils.promiseSearchComplete(window);
    Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0);
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.ok(details.autofill);
    Assert.equal(details.url, "http://example.com/");
    Assert.equal(gURLBar.textValue, "example.com/");
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

add_task(async function userTyping() {
  await promiseAutocompleteResultPopup("foo", window, true);
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(details.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.ok(details.searchParams);
  Assert.equal(details.searchParams.query, "foo");
  Assert.equal(gURLBar.textValue, "foo");
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function empty() {
  await promiseAutocompleteResultPopup("", window, true);
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), -1);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(details.url, "http://example.com/");
  Assert.equal(gURLBar.textValue, "");
});
