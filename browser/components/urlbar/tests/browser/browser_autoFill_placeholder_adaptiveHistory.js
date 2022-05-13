/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the autofill placeholder with adaptive history.

"use strict";

add_task(async function() {
  const win = await BrowserTestUtils.openNewBrowserWindow();

  UrlbarPrefs.set("autoFill.adaptiveHistory.enabled", true);

  await PlacesTestUtils.addVisits([{ uri: "http://example.com/test" }]);
  await UrlbarUtils.addToInputHistory("http://example.com/test", "exa");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "exa",
    fireInputEvent: true,
  });

  const details = await UrlbarTestUtils.getDetailsOfResultAt(win, 0);
  Assert.ok(details.autofill);
  Assert.equal(win.gURLBar.value, "example.com/test");
  Assert.equal(win.gURLBar.selectionStart, "exa".length);
  Assert.equal(win.gURLBar.selectionEnd, "example.com/test".length);

  await searchAndCheck("e", "example.com/test", win);
  await searchAndCheck("ex", "example.com/test", win);
  await searchAndCheck("exa", "example.com/test", win);

  UrlbarPrefs.clear("autoFill.adaptiveHistory.enabled");
  await cleanUp(win);
  await BrowserTestUtils.closeWindow(win);
});

async function searchAndCheck(searchString, expectedAutofillValue, win) {
  win.gURLBar.value = searchString;

  // Placeholder autofill is done on input, so fire an input event.  As the
  // comment at the top of this file says, we can't use
  // promiseAutocompleteResultPopup() or other helpers that wait for searches to
  // complete because we are specifically checking autofill before the search
  // completes.
  UrlbarTestUtils.fireInputEvent(win);

  // Check the input value and selection immediately, before waiting on the
  // search to complete.
  Assert.equal(win.gURLBar.value, expectedAutofillValue);
  Assert.equal(win.gURLBar.selectionStart, searchString.length);
  Assert.equal(win.gURLBar.selectionEnd, expectedAutofillValue.length);

  await UrlbarTestUtils.promiseSearchComplete(win);
}

async function cleanUp(win) {
  await UrlbarTestUtils.promisePopupClose(win, () => win.gURLBar.blur());
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
