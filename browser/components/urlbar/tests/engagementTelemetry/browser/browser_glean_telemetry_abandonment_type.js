/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of abandonment telemetry.
// - reason

function checkUrlbarFocus(win, focusState) {
  let urlbar = win.gURLBar;
  is(
    focusState ? win.document.activeElement : !win.document.activeElement,
    focusState ? urlbar.inputField : !urlbar.inputField,
    `URL Bar should ${focusState ? "" : "not "}be focused`
  );
}

// Tests that a tab switch from a focused URL bar to another tab with a focused
// URL bar records the correct abandonment telemetry with abandonment type
// "tab_swtich".
add_task(async function tabSwitchFocusedToFocused() {
  await doTest(async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test search",
    });
    checkUrlbarFocus(window, true);

    let promiseTabOpened = BrowserTestUtils.waitForEvent(
      gBrowser.tabContainer,
      "TabOpen"
    );
    EventUtils.synthesizeMouseAtCenter(gBrowser.tabContainer.newTabButton, {});
    let openEvent = await promiseTabOpened;
    let tab2 = openEvent.target;
    checkUrlbarFocus(window, true);

    await assertAbandonmentTelemetry([{ abandonment_type: "tab_switch" }]);

    await BrowserTestUtils.removeTab(tab2);
  });
});

// Verifies that switching from a tab with a focused URL bar to a tab where the
// URL bar loses focus logs abandonment telemetry with abandonment type
// "blur".
add_task(async function tabSwitchFocusedToUnfocused() {
  await doTest(async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test search",
    });
    checkUrlbarFocus(window, true);

    let tab2 = await BrowserTestUtils.openNewForegroundTab(window.gBrowser);
    checkUrlbarFocus(window, false);

    await assertAbandonmentTelemetry([{ abandonment_type: "blur" }]);

    await BrowserTestUtils.removeTab(tab2);
  });
});

// Ensures that switching from a tab with an unfocused URL bar to a tab where
// the URL bar gains focus does not record any abandonment telemetry, reflecting
// no change in focus state relevant to abandonment.
add_task(async function tabSwitchUnFocusedToFocused() {
  await doTest(async () => {
    checkUrlbarFocus(window, false);

    let promiseTabOpened = BrowserTestUtils.waitForEvent(
      gBrowser.tabContainer,
      "TabOpen"
    );
    EventUtils.synthesizeMouseAtCenter(gBrowser.tabContainer.newTabButton, {});
    let openEvent = await promiseTabOpened;
    let tab2 = openEvent.target;
    checkUrlbarFocus(window, true);

    const telemetries = Glean.urlbar.abandonment.testGetValue() ?? [];
    Assert.equal(
      telemetries.length,
      0,
      "Telemetry event length matches expected event length."
    );

    await BrowserTestUtils.removeTab(tab2);
  });
});

// Checks that switching between two tabs, both with unfocused URL bars, does
// not trigger any abandonment telmetry.
add_task(async function tabSwitchUnFocusedToUnFocused() {
  await doTest(async () => {
    checkUrlbarFocus(window, false);

    let tab2 = await BrowserTestUtils.openNewForegroundTab(window.gBrowser);
    checkUrlbarFocus(window, false);

    const telemetries = Glean.urlbar.abandonment.testGetValue() ?? [];
    Assert.equal(
      telemetries.length,
      0,
      "Telemetry event length matches expected event length."
    );

    await BrowserTestUtils.removeTab(tab2);
  });
});
