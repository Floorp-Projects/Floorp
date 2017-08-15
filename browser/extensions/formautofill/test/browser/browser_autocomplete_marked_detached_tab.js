/**
 * Test that autofill autocomplete works after detaching a tab
 */

"use strict";

const URL = BASE_URL + "autocomplete_basic.html";

function checkPopup(autoCompletePopup) {
  let first = autoCompletePopup.view.results[0];
  const {primary, secondary} = JSON.parse(first.label);
  ok(primary.startsWith(TEST_ADDRESS_1["street-address"].split("\n")[0]),
     "Check primary label is street address");
  is(secondary, TEST_ADDRESS_1["address-level2"], "Check secondary label is address-level2");
}

add_task(async function setup_storage() {
  await saveAddress(TEST_ADDRESS_1);
  await saveAddress(TEST_ADDRESS_2);
  await saveAddress(TEST_ADDRESS_3);
});

add_task(async function test_detach_tab_marked() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async function(browser) {
    const {autoCompletePopup} = browser;

    // Check the page after the initial load
    await openPopupOn(browser, "#street-address");
    checkPopup(autoCompletePopup);
    await closePopup(browser);

    // Detach the tab to a new window
    let newWin = gBrowser.replaceTabWithWindow(gBrowser.getTabForBrowser(browser));
    await TestUtils.topicObserved("browser-delayed-startup-finished", subject => {
      return subject == newWin;
    });

    info("tab was detached");
    let newBrowser = newWin.gBrowser.selectedBrowser;
    ok(newBrowser, "Found new <browser>");
    let newAutoCompletePopup = newBrowser.autoCompletePopup;
    ok(newAutoCompletePopup, "Found new autocomplete popup");

    await openPopupOn(newBrowser, "#street-address");
    checkPopup(newAutoCompletePopup);

    await closePopup(newBrowser);
    await BrowserTestUtils.closeWindow(newWin);
  });
});
