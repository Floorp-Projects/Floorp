/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the autofill placeholder with adaptive history.

"use strict";

add_task(async function() {
  UrlbarPrefs.set("autoFill.adaptiveHistory.enabled", true);

  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits(["http://example.com/test"]);
  await UrlbarUtils.addToInputHistory("http://example.com/test", "exa");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exa",
    fireInputEvent: true,
  });

  const details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "example.com/test");
  Assert.equal(gURLBar.selectionStart, "exa".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/test".length);

  await searchAndCheck("e", "example.com/", window);
  await searchAndCheck("ex", "example.com/", window);
  await searchAndCheck("exa", "example.com/test", window);
  await searchAndCheck("exam", "example.com/test", window);
  await searchAndCheck("example.com", "example.com/test", window);
  await searchAndCheck("example.com/", "example.com/test", window);
  await searchAndCheck("example.com/t", "example.com/test", window);
  await searchAndCheck("example.com/test", "example.com/test", window);

  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  await PlacesUtils.history.clear();
  UrlbarPrefs.clear("autoFill.adaptiveHistory.enabled");
});

async function searchAndCheck(searchString, expectedAutofillValue, win) {
  win.gURLBar.value = "";
  win.gURLBar.value = searchString;

  // Placeholder autofill is done on input, so fire an input event.  As the
  // comment at the top of this file says, we can't use
  // promiseAutocompleteResultPopup() or other helpers that wait for searches to
  // complete because we are specifically checking autofill before the search
  // completes.
  UrlbarTestUtils.fireInputEvent(win);

  await UrlbarTestUtils.promiseSearchComplete(win);

  // Check the input value and selection immediately, before waiting on the
  // search to complete.
  Assert.equal(win.gURLBar.value, expectedAutofillValue);
  Assert.equal(win.gURLBar.selectionStart, searchString.length);
  Assert.equal(win.gURLBar.selectionEnd, expectedAutofillValue.length);
}
