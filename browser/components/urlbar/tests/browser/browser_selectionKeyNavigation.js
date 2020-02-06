/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that the up/down, page-up/down, and tab keys properly
// adjust the selection.  See also browser_caret_navigation.js.

"use strict";

const MAX_RESULTS = UrlbarPrefs.get("maxRichResults");

add_task(async function init() {
  for (let i = 0; i < MAX_RESULTS; i++) {
    await PlacesTestUtils.addVisits("http://example.com/" + i);
  }
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });
});

add_task(async function downKey() {
  await promiseAutocompleteResultPopup("exam", window, true);
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    0,
    "The heuristic autofill result should be selected initially"
  );
  for (let i = 1; i < MAX_RESULTS; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), i);
  }
  EventUtils.synthesizeKey("KEY_ArrowDown");
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window);
  Assert.ok(oneOffs.selectedButton, "A one-off should now be selected");
  while (oneOffs.selectedButton) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    0,
    "The heuristic autofill result should be selected again"
  );
});

add_task(async function upKey() {
  await promiseAutocompleteResultPopup("exam", window, true);
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    0,
    "The heuristic autofill result should be selected initially"
  );
  EventUtils.synthesizeKey("KEY_ArrowUp");
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window);
  Assert.ok(oneOffs.selectedButton, "A one-off should now be selected");
  while (oneOffs.selectedButton) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
  }
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    MAX_RESULTS - 1,
    "The last result should be selected"
  );
  for (let i = 1; i < MAX_RESULTS; i++) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      MAX_RESULTS - i - 1
    );
  }
});

add_task(async function pageDownKey() {
  await promiseAutocompleteResultPopup("exam", window, true);
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    0,
    "The heuristic autofill result should be selected initially"
  );
  let pageCount = Math.ceil((MAX_RESULTS - 1) / UrlbarUtils.PAGE_UP_DOWN_DELTA);
  for (let i = 0; i < pageCount; i++) {
    EventUtils.synthesizeKey("KEY_PageDown");
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      Math.min((i + 1) * UrlbarUtils.PAGE_UP_DOWN_DELTA, MAX_RESULTS - 1)
    );
  }
  EventUtils.synthesizeKey("KEY_PageDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    0,
    "Page down at end should wrap around to first result"
  );
});

add_task(async function pageUpKey() {
  await promiseAutocompleteResultPopup("exam", window, true);
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    0,
    "The heuristic autofill result should be selected initially"
  );
  EventUtils.synthesizeKey("KEY_PageUp");
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    MAX_RESULTS - 1,
    "Page up at start should wrap around to last result"
  );
  let pageCount = Math.ceil((MAX_RESULTS - 1) / UrlbarUtils.PAGE_UP_DOWN_DELTA);
  for (let i = 0; i < pageCount; i++) {
    EventUtils.synthesizeKey("KEY_PageUp");
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      Math.max(MAX_RESULTS - 1 - (i + 1) * UrlbarUtils.PAGE_UP_DOWN_DELTA, 0)
    );
  }
});

add_task(async function pageDownKeyShowsView() {
  await promiseAutocompleteResultPopup("exam", window, true);
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_PageDown");
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.ok(UrlbarTestUtils.isPopupOpen(window));
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), 0);
});

add_task(async function pageUpKeyShowsView() {
  await promiseAutocompleteResultPopup("exam", window, true);
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_PageUp");
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.ok(UrlbarTestUtils.isPopupOpen(window));
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), 0);
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
  await promiseAutocompleteResultPopup("exam", window, true);
  await UrlbarTestUtils.promisePopupClose(window);
  Assert.equal(document.activeElement, gURLBar.inputField);
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.notEqual(document.activeElement, gURLBar.inputField);
});

add_task(async function tabKeyRestrictedMouse() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update1", true],
      ["browser.urlbar.update1.restrictTabAfterKeyboardFocus", true],
      ["browser.urlbar.openViewOnFocus", true],
    ],
  });

  await populateAndReopenUrlbarView(/* useKeyboardShortcut */ false);
  await tabThroughResults();
  await SpecialPowers.popPrefEnv();
});

add_task(async function tabKeyRestrictedKeyboard() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update1", true],
      ["browser.urlbar.update1.restrictTabAfterKeyboardFocus", true],
      ["browser.urlbar.openViewOnFocus", true],
    ],
  });
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

  await SpecialPowers.popPrefEnv();
});

add_task(async function tabKeyRestrictedKeyboardThenMouse() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update1", true],
      ["browser.urlbar.update1.restrictTabAfterKeyboardFocus", true],
      ["browser.urlbar.openViewOnFocus", true],
    ],
  });

  await populateAndReopenUrlbarView(/* useKeyboardShortcut */ true);

  // Going by the previous subtest, TAB should not move the results. Now we
  // click on the input to reenable TAB.
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  await UrlbarTestUtils.promiseSearchComplete(window);

  await tabThroughResults();

  await SpecialPowers.popPrefEnv();
});

add_task(async function tabKeyRestrictedMouseThenKeyboard() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update1", true],
      ["browser.urlbar.update1.restrictTabAfterKeyboardFocus", true],
      ["browser.urlbar.openViewOnFocus", true],
    ],
  });

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
  await SpecialPowers.popPrefEnv();
});

// Testing that focusing with the mouse then the keyboard still allows tabbing
// through the toolbar even when openViewOnFocus is disabled.
add_task(async function tabKeyRestrictedNoOpenViewOnFocus() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update1", true],
      ["browser.urlbar.update1.restrictTabAfterKeyboardFocus", true],
      ["browser.urlbar.openViewOnFocus", false],
    ],
  });

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

  await SpecialPowers.popPrefEnv();
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

  await SpecialPowers.popPrefEnv();
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
