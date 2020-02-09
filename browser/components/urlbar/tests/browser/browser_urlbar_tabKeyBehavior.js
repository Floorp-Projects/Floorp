/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that tab keys properly adjust the selection given that
// the appropriate prefs are enabled.

"use strict";

const MAX_RESULTS = UrlbarPrefs.get("maxRichResults");

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update1", true],
      ["browser.urlbar.update1.restrictTabAfterKeyboardFocus", true],
      ["browser.urlbar.openViewOnFocus", true],
    ],
  });

  for (let i = 0; i < MAX_RESULTS; i++) {
    await PlacesTestUtils.addVisits("http://example.com/" + i);
  }

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    await SpecialPowers.popPrefEnv();
  });
});

add_task(async function tabKey() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update1.restrictTabAfterKeyboardFocus", false]],
  });

  await promiseAutocompleteResultPopup("exam", window, true);
  await tabThroughResults();
  await SpecialPowers.popPrefEnv();
});

add_task(async function tabKeyReverse() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update1.restrictTabAfterKeyboardFocus", false]],
  });

  await promiseAutocompleteResultPopup("exam", window, true);
  await tabThroughResults(/* reverse */ true);
  await SpecialPowers.popPrefEnv();
});

add_task(async function tabKeyBlur() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update1.restrictTabAfterKeyboardFocus", false]],
  });

  await promiseAutocompleteResultPopup("exam", window, true);
  await UrlbarTestUtils.promisePopupClose(window);
  Assert.equal(document.activeElement, gURLBar.inputField);
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.notEqual(document.activeElement, gURLBar.inputField);
  await SpecialPowers.popPrefEnv();
});

add_task(async function tabKeyRestrictedMouse() {
  await populateAndReopenUrlbarView(/* useKeyboardShortcut */ false);
  await tabThroughResults();
});

add_task(async function tabKeyRestrictedKeyboard() {
  await populateAndReopenUrlbarView(/* useKeyboardShortcut */ true);
  let focusPromise = waitForFocusOnNextFocusableElement();
  EventUtils.synthesizeKey("KEY_Tab");
  await focusPromise;

  Assert.ok(
    !UrlbarTestUtils.isPopupOpen(window),
    "UrlbarView should be closed."
  );
});

add_task(async function tabKeyRestrictedKeyboardThenMouse() {
  await populateAndReopenUrlbarView(/* useKeyboardShortcut */ true);

  // Going by the previous subtest, TAB should not move the results. Now we
  // click on the input to reenable TAB.
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  await UrlbarTestUtils.promiseSearchComplete(window);

  await tabThroughResults();
});

add_task(async function tabKeyRestrictedMouseThenKeyboard() {
  // First we test focusing the Urlbar with the mouse. The user should be able
  // to TAB through results.
  await populateAndReopenUrlbarView(/* useKeyboardShortcut */ false);
  await tabThroughResults();

  // Then we test focusing the Urlbar with the keyboard. The user should not be
  // able to TAB through results.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeKey("l", { accelKey: true });
  });
  await UrlbarTestUtils.promiseSearchComplete(window);

  let focusPromise = waitForFocusOnNextFocusableElement();
  EventUtils.synthesizeKey("KEY_Tab");
  await focusPromise;

  Assert.ok(
    !UrlbarTestUtils.isPopupOpen(window),
    "UrlbarView should be closed."
  );
});

// Testing that focusing with the mouse then the keyboard still allows tabbing
// through the toolbar even when openViewOnFocus is disabled.
add_task(async function tabKeyRestrictedNoOpenViewOnFocus() {
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  gURLBar.blur();

  window.document.getElementById("Browser:OpenLocation").doCommand();
  Assert.ok(gURLBar.focused, "The Urlbar should be focused");

  let focusPromise = waitForFocusOnNextFocusableElement();
  EventUtils.synthesizeKey("KEY_Tab");
  await focusPromise;

  Assert.ok(
    !UrlbarTestUtils.isPopupOpen(window),
    "UrlbarView should be closed."
  );
});

async function tabThroughResults(reverse = false) {
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    0,
    "The heuristic autofill result should be selected initially."
  );

  for (let i = 1; i < MAX_RESULTS; i++) {
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: reverse });
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      reverse ? MAX_RESULTS - i : i
    );
  }

  EventUtils.synthesizeKey("KEY_Tab");

  if (!reverse) {
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      0,
      "The heuristic autofill result should be selected again."
    );
  }

  await UrlbarTestUtils.promisePopupClose(window, () => {
    window.gURLBar.blur();
  });
}

/**
 * Populates the Urlbar with results then reopens the view. We expect that the
 * results will persist in the second opening due to the Urlbar's retained
 * results feature. This helper expects that the pref
 * "browser.urlbar.openViewOnFocus" be `true`.
 * @param {boolean} useKeyboardShortcut
 *   If true, the Urlbar is opened with a keyboard shortcut. Otherwise, the
 *   Urlbar is opened with a click.
 */
async function populateAndReopenUrlbarView(useKeyboardShortcut) {
  // Populate the view with results.
  await promiseAutocompleteResultPopup("exam", window, true);
  Assert.equal(UrlbarTestUtils.getResultCount(window), MAX_RESULTS);
  await UrlbarTestUtils.promisePopupClose(window, () => {
    window.gURLBar.blur();
  });

  // Reopen the view knowing we'll have MAX_RESULTS results because of retained
  // results.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    if (useKeyboardShortcut) {
      EventUtils.synthesizeKey("l", { accelKey: true });
    } else {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    }
  });
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(UrlbarTestUtils.getResultCount(window), MAX_RESULTS);
}

async function waitForFocusOnNextFocusableElement() {
  let nextFocusableElement = document.getElementById("urlbar-container")
    .nextElementSibling;
  while (
    nextFocusableElement &&
    (!nextFocusableElement.classList.contains("toolbarbutton-1") ||
      nextFocusableElement.hasAttribute("hidden"))
  ) {
    nextFocusableElement = nextFocusableElement.nextElementSibling;
  }

  Assert.ok(
    nextFocusableElement.classList.contains("toolbarbutton-1"),
    "We should have a reference to the next focusable element after the Urlbar."
  );

  return BrowserTestUtils.waitForCondition(() => {
    return nextFocusableElement.tabIndex == -1;
  });
}
