/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Checks that pressing the down arrow key starts the proper searches, depending
// on the input value/state.

"use strict";

add_setup(async function () {
  await PlacesUtils.history.clear();
  // Enough vists to get this site into Top Sites.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits("http://example.com/");
  }

  await updateTopSites(
    sites => sites && sites[0] && sites[0].url == "http://example.com/"
  );
  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
  });
});

add_task(async function url() {
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    gURLBar.focus();
    gURLBar.selectionEnd = gURLBar.untrimmedValue.length;
    gURLBar.selectionStart = gURLBar.untrimmedValue.length;
    EventUtils.synthesizeKey("KEY_ArrowDown");
    await UrlbarTestUtils.promiseSearchComplete(window);
    Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), 0);
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.ok(details.autofill);
    Assert.equal(details.url, "http://example.com/");
    Assert.equal(
      gURLBar.value,
      UrlbarTestUtils.trimURL("http://example.com/", {
        removeSingleTrailingSlash: false,
      })
    );
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

add_task(async function userTyping() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), 0);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(details.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.ok(details.searchParams);
  Assert.equal(details.searchParams.query, "foo");
  Assert.equal(gURLBar.value, "foo");
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function empty() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), -1);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(details.url, "http://example.com/");
  Assert.equal(gURLBar.value, "");
});

add_task(async function new_window() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  win.gURLBar.focus();
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  await UrlbarTestUtils.promiseSearchComplete(win);
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(win), -1);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(win, 0);
  Assert.equal(details.url, "http://example.com/");
  Assert.equal(win.gURLBar.value, "");
  await BrowserTestUtils.closeWindow(win);
});
