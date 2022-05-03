/**
 * Test that autofill autocomplete works after detaching a tab
 */

"use strict";

const URL = BASE_URL + "autocomplete_basic.html";

function checkPopup(autoCompletePopup) {
  let first = autoCompletePopup.view.results[0];
  const { primary, secondary } = JSON.parse(first.label);
  ok(
    primary.startsWith(TEST_ADDRESS_1["street-address"].split("\n")[0]),
    "Check primary label is street address"
  );
  is(
    secondary,
    TEST_ADDRESS_1["address-level2"],
    "Check secondary label is address-level2"
  );
}

add_task(async function setup_storage() {
  await setStorage(TEST_ADDRESS_1, TEST_ADDRESS_2, TEST_ADDRESS_3);
});

add_task(async function test_detach_tab_marked() {
  let tab = await BrowserTestUtils.openNewForegroundTab({ gBrowser, url: URL });
  let browser = tab.linkedBrowser;
  const { autoCompletePopup } = browser;

  // Check the page after the initial load
  await openPopupOn(browser, "#street-address");
  checkPopup(autoCompletePopup);
  await closePopup(browser);

  // Detach the tab to a new window
  info("expecting tab replaced with new window");
  let windowLoadedPromise = BrowserTestUtils.waitForNewWindow();
  let newWin = gBrowser.replaceTabWithWindow(
    gBrowser.getTabForBrowser(browser)
  );
  await windowLoadedPromise;

  info("tab was detached");
  let newBrowser = newWin.gBrowser.selectedBrowser;
  ok(newBrowser, "Found new <browser>");
  let newAutoCompletePopup = newBrowser.autoCompletePopup;
  ok(newAutoCompletePopup, "Found new autocomplete popup");

  await openPopupOn(newBrowser, "#street-address");
  checkPopup(newAutoCompletePopup);

  await closePopup(newBrowser);
  let windowRefocusedPromise = BrowserTestUtils.waitForEvent(window, "focus");
  await BrowserTestUtils.closeWindow(newWin);
  await windowRefocusedPromise;
});
