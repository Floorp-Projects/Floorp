/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test ensures that pressing ctrl+enter bypasses the autoFilled
 * value, and only considers what the user typed (but not just enter).
 */

async function test_autocomplete(data) {
  let { desc, typed, autofilled, modified, waitForUrl, keys } = data;
  info(desc);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typed,
  });
  Assert.equal(gURLBar.value, autofilled, "autofilled value is as expected");

  let promiseLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
    waitForUrl,
    gBrowser.selectedBrowser
  );

  keys.forEach(([key, mods]) => EventUtils.synthesizeKey(key, mods));

  Assert.equal(gURLBar.value, modified, "value is as expected");

  await promiseLoad;
  gURLBar.blur();
}

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", true]],
  });
  registerCleanupFunction(async function () {
    gURLBar.handleRevert();
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  });

  // Add a typed visit, so it will be autofilled.
  await PlacesTestUtils.addVisits({
    uri: "https://example.com/",
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  await test_autocomplete({
    desc: "ENTER on the autofilled part should use autofill",
    typed: "exam",
    autofilled: "example.com/",
    modified: UrlbarTestUtils.trimURL("https://example.com"),
    waitForUrl: "https://example.com/",
    keys: [["KEY_Enter"]],
  });

  await test_autocomplete({
    desc: "CTRL+ENTER on the autofilled part should bypass autofill",
    typed: "exam",
    autofilled: "example.com/",
    modified: UrlbarTestUtils.trimURL("https://www.exam.com"),
    waitForUrl: "https://www.exam.com/",
    keys: [["KEY_Enter", { ctrlKey: true }]],
  });
});
