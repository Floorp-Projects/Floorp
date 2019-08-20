/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test ensures that pressing ctrl+enter bypasses the autoFilled
 * value, and only considers what the user typed (but not just enter).
 */

async function test_autocomplete(data) {
  let { desc, typed, autofilled, modified, waitForUrl, keys } = data;
  info(desc);

  await promiseAutocompleteResultPopup(typed);
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

add_task(async function() {
  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
    gURLBar.handleRevert();
    await PlacesUtils.history.clear();
  });
  Services.prefs.setBoolPref("browser.urlbar.autoFill", true);

  // Add a typed visit, so it will be autofilled.
  await PlacesTestUtils.addVisits({
    uri: "http://example.com/",
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  });

  await test_autocomplete({
    desc: "CTRL+ENTER on the autofilled part should use autofill",
    typed: "exam",
    autofilled: "example.com/",
    modified: "example.com",
    waitForUrl: "http://example.com/",
    keys: [["KEY_Enter"]],
  });

  await test_autocomplete({
    desc: "CTRL+ENTER on the autofilled part should bypass autofill",
    typed: "exam",
    autofilled: "example.com/",
    modified: "www.exam.com",
    waitForUrl: "http://www.exam.com/",
    keys: [["KEY_Enter", { ctrlKey: true }]],
  });
});
