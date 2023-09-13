/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that autofill is cleared if a remote search mode is entered but still
 * works for local search modes.
 */

"use strict";

add_setup(async function () {
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits([{ uri: "http://example.com/" }]);
  }
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });
  let defaultEngine = Services.search.getEngineByName("Example");
  await Services.search.moveEngine(defaultEngine, 0);

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.quickactions", false]],
  });

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

// Tests that autofill is cleared when entering a remote search mode and that
// autofill doesn't happen when in that mode.
add_task(async function remote() {
  info("Sanity check: we autofill in a normal search.");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "ex",
    fireInputEvent: true,
  });
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill, "We're autofilling.");
  Assert.equal(
    gURLBar.value,
    "example.com/",
    "Urlbar contains the autofilled URL."
  );
  info("Enter remote search mode and check autofill is cleared.");
  await UrlbarTestUtils.enterSearchMode(window);
  Assert.equal(gURLBar.value, "ex", "Urlbar contains the typed string.");

  info("Continue typing and check that we're not autofilling.");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exa",
    fireInputEvent: true,
  });

  details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(!details.autofill, "We're not autofilling.");
  Assert.equal(gURLBar.value, "exa", "Urlbar contains the typed string.");

  info("Exit remote search mode and check that we now autofill.");
  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exam",
    fireInputEvent: true,
  });
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill, "We're autofilling.");
  Assert.equal(
    gURLBar.value,
    "example.com/",
    "Urlbar contains the typed string."
  );
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
});

// Tests that autofill works as normal when entering and when in a local search
// mode.
add_task(async function local() {
  info("Sanity check: we autofill in a normal search.");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "ex",
    fireInputEvent: true,
  });
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill, "We're autofilling.");
  Assert.equal(
    gURLBar.value,
    "example.com/",
    "Urlbar contains the autofilled URL."
  );
  info("Enter local search mode and check autofill is preserved.");
  await UrlbarTestUtils.enterSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
  });
  Assert.equal(
    gURLBar.value,
    "example.com/",
    "Urlbar contains the autofilled URL."
  );

  info("Continue typing and check that we're autofilling.");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exa",
    fireInputEvent: true,
  });

  details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill, "We're autofilling.");
  Assert.equal(
    gURLBar.value,
    "example.com/",
    "Urlbar contains the autofilled URL."
  );

  info("Exit local search mode and check that nothing has changed.");
  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exam",
    fireInputEvent: true,
  });
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill, "We're autofilling.");
  Assert.equal(
    gURLBar.value,
    "example.com/",
    "Urlbar contains the typed string."
  );
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
});
