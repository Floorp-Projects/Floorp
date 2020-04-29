/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that the tab key properly adjusts the selection or moves
// through toolbar items, depending on the urlbar state.
// When the view is open, tab should go through results if the urlbar was
// focused with the mouse, or has a typed string.

"use strict";

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.openViewOnFocus", true]],
  });

  for (let i = 0; i < UrlbarPrefs.get("maxRichResults"); i++) {
    await PlacesTestUtils.addVisits("http://example.com/" + i);
  }

  registerCleanupFunction(PlacesUtils.history.clear);
});

add_task(async function tabWithSearchString() {
  info("Tab with a search string");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "exam",
    fireInputEvent: true,
  });
  await expectTabThroughResults();
  info("Reverse Tab with a search string");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "exam",
    fireInputEvent: true,
  });
  await expectTabThroughResults({ reverse: true });
});

add_task(async function tabNoSearchString() {
  info("Tab without a search string");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "",
    fireInputEvent: true,
  });
  await expectTabThroughToolbar();
  info("Reverse Tab without a search string");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "",
    fireInputEvent: true,
  });
  await expectTabThroughToolbar({ reverse: true });
});

add_task(async function tabAfterBlur() {
  info("Tab after closing the view");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "exam",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.promisePopupClose(window);
  await expectTabThroughToolbar();
});

add_task(async function tabNoSearchStringMouseFocus() {
  info("Tab in a new blank tab after mouse focus");
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    });
    await UrlbarTestUtils.promiseSearchComplete(window);
    await expectTabThroughResults();
  });
  info("Tab in a loaded tab after mouse focus");
  await BrowserTestUtils.withNewTab("example.com", async () => {
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    });
    await UrlbarTestUtils.promiseSearchComplete(window);
    await expectTabThroughResults();
  });
});

add_task(async function tabNoSearchStringKeyboardFocus() {
  info("Tab in a new blank tab after keyboard focus");
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeKey("l", { accelKey: true });
    });
    await UrlbarTestUtils.promiseSearchComplete(window);
    await expectTabThroughToolbar();
  });
  info("Tab in a loaded tab after keyboard focus");
  await BrowserTestUtils.withNewTab("example.com", async () => {
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeKey("l", { accelKey: true });
    });
    await UrlbarTestUtils.promiseSearchComplete(window);
    await expectTabThroughToolbar();
  });
});

add_task(async function tabRetainedResultMouseFocus() {
  info("Tab after retained results with mouse focus");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "exam",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });
  await UrlbarTestUtils.promiseSearchComplete(window);
  await expectTabThroughResults();
});

add_task(async function tabRetainedResultsKeyboardFocus() {
  info("Tab after retained results with keyboard focus");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "exam",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeKey("l", { accelKey: true });
  });
  await UrlbarTestUtils.promiseSearchComplete(window);
  await expectTabThroughResults();
});

add_task(async function tabRetainedResults() {
  info("Tab with a search string after mouse focus.");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "exam",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  await expectTabThroughResults();
  await SpecialPowers.popPrefEnv();
});

async function expectTabThroughResults(options = { reverse: false }) {
  let resultCount = UrlbarTestUtils.getResultCount(window);
  Assert.ok(resultCount > 0, "There should be results");

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

  let initiallySelectedIndex = result.heuristic ? 0 : -1;
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    initiallySelectedIndex,
    "Check the initial selection."
  );

  for (let i = initiallySelectedIndex + 1; i < resultCount; i++) {
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: options.reverse });
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      options.reverse ? resultCount - i : i
    );
  }

  EventUtils.synthesizeKey("KEY_Tab");

  if (!options.reverse) {
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      initiallySelectedIndex,
      "Should be back at the initial selection."
    );
  }

  await UrlbarTestUtils.promisePopupClose(window, () => {
    gURLBar.blur();
  });
}

async function expectTabThroughToolbar(options = { reverse: false }) {
  if (gURLBar.getAttribute("pageproxystate") == "valid") {
    Assert.equal(document.activeElement, gURLBar.inputField);
    EventUtils.synthesizeKey("KEY_Tab");
    Assert.notEqual(document.activeElement, gURLBar.inputField);
  } else {
    let focusPromise = waitForFocusOnNextFocusableElement(options.reverse);
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: options.reverse });
    await focusPromise;
  }
  Assert.ok(!gURLBar.view.isOpen, "The urlbar view should be closed.");
}

async function waitForFocusOnNextFocusableElement(reverse = false) {
  let urlbar = document.getElementById("urlbar-container");
  let nextFocusableElement = reverse
    ? urlbar.previousElementSibling
    : urlbar.nextElementSibling;
  info(nextFocusableElement);
  while (
    nextFocusableElement &&
    (!nextFocusableElement.classList.contains("toolbarbutton-1") ||
      nextFocusableElement.hasAttribute("hidden"))
  ) {
    nextFocusableElement = reverse
      ? nextFocusableElement.previousElementSibling
      : nextFocusableElement.nextElementSibling;
  }

  Assert.ok(
    nextFocusableElement.classList.contains("toolbarbutton-1"),
    "We should have a reference to the next focusable element after the Urlbar."
  );

  return BrowserTestUtils.waitForCondition(
    () => nextFocusableElement.tabIndex == -1
  );
}
