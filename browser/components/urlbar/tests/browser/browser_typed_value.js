/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test ensures that the urlbar is restored to the typed value on blur.

"use strict";

add_setup(async function() {
  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
    gURLBar.handleRevert();
    await PlacesUtils.history.clear();
  });
  Services.prefs.setBoolPref("browser.urlbar.autoFill", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);

  await PlacesTestUtils.addVisits([
    "http://example.com/",
    "http://example.com/foo",
  ]);
});

add_task(async function test_autofill() {
  let typed = "ex";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typed,
    fireInputEvent: true,
  });
  Assert.equal(gURLBar.value, "example.com/", "autofilled value as expected");
  Assert.equal(gURLBar.selectionStart, typed.length);
  Assert.equal(gURLBar.selectionEnd, gURLBar.value.length);

  gURLBar.blur();
  Assert.equal(gURLBar.value, typed, "Value should have been restored");
  gURLBar.focus();
  Assert.equal(gURLBar.value, typed, "Value should not have changed");
  Assert.equal(gURLBar.selectionStart, typed.length);
  Assert.equal(gURLBar.selectionEnd, typed.length);
});

add_task(async function test_complete_selection() {
  let typed = "ex";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typed,
    fireInputEvent: true,
  });
  Assert.equal(gURLBar.value, "example.com/", "autofilled value as expected");
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "Should have the correct number of matches"
  );
  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    gURLBar.value,
    "example.com/foo",
    "Value should have been completed"
  );

  gURLBar.blur();
  Assert.equal(gURLBar.value, typed, "Value should have been restored");
  gURLBar.focus();
  Assert.equal(gURLBar.value, typed, "Value should not have changed");
  Assert.equal(gURLBar.selectionStart, typed.length);
  Assert.equal(gURLBar.selectionEnd, typed.length);
});
