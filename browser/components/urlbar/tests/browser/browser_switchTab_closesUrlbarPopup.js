/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Checks that switching tabs closes the urlbar popup.
 */

"use strict";

add_task(async function() {
  let tab1 = BrowserTestUtils.addTab(gBrowser);
  let tab2 = BrowserTestUtils.addTab(gBrowser);

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    BrowserTestUtils.removeTab(tab1);
    BrowserTestUtils.removeTab(tab2);
  });

  // Add a couple of dummy entries to ensure the history popup will open.
  await PlacesTestUtils.addVisits([
    { uri: makeURI("http://example.com/foo") },
    { uri: makeURI("http://example.com/foo/bar") },
  ]);

  // When urlbar in a new tab is focused, and a tab switch occurs,
  // the urlbar popup should be closed
  await BrowserTestUtils.switchTab(gBrowser, tab2);
  gURLBar.focus(); // focus the urlbar in the tab we will switch to
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  // Now open the popup by the history marker.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeMouseAtCenter(gURLBar.dropmarker, {}, window);
  });
  // Check that the popup closes when we switch tab.
  await UrlbarTestUtils.promisePopupClose(window, () => {
    return BrowserTestUtils.switchTab(gBrowser, tab2);
  });
  Assert.ok(true, "Popup was successfully closed");
});
