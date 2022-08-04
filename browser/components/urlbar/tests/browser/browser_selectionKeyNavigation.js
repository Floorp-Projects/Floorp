/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that the up/down and page-up/down properly adjust the
// selection.  See also browser_caret_navigation.js and
// browser_urlbar_tabKeyBehavior.js.

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
  for (const ctrlKey of [false, true]) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "exam",
      fireInputEvent: true,
    });
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      0,
      "The heuristic autofill result should be selected initially"
    );
    for (let i = 1; i < MAX_RESULTS; i++) {
      EventUtils.synthesizeKey("KEY_ArrowDown", { ctrlKey });
      Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), i);
    }
    EventUtils.synthesizeKey("KEY_ArrowDown", { ctrlKey });
    let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window);
    Assert.ok(oneOffs.selectedButton, "A one-off should now be selected");
    while (oneOffs.selectedButton) {
      EventUtils.synthesizeKey("KEY_ArrowDown", { ctrlKey });
    }
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      0,
      "The heuristic autofill result should be selected again"
    );
  }
});

add_task(async function upKey() {
  for (const ctrlKey of [false, true]) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "exam",
      fireInputEvent: true,
    });
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      0,
      "The heuristic autofill result should be selected initially"
    );
    EventUtils.synthesizeKey("KEY_ArrowUp", { ctrlKey });
    let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window);
    Assert.ok(oneOffs.selectedButton, "A one-off should now be selected");
    while (oneOffs.selectedButton) {
      EventUtils.synthesizeKey("KEY_ArrowUp", { ctrlKey });
    }
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      MAX_RESULTS - 1,
      "The last result should be selected"
    );
    for (let i = 1; i < MAX_RESULTS; i++) {
      EventUtils.synthesizeKey("KEY_ArrowUp", { ctrlKey });
      Assert.equal(
        UrlbarTestUtils.getSelectedRowIndex(window),
        MAX_RESULTS - i - 1
      );
    }
  }
});

add_task(async function pageDownKey() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exam",
    fireInputEvent: true,
  });
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
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exam",
    fireInputEvent: true,
  });
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
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exam",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_PageDown");
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.ok(UrlbarTestUtils.isPopupOpen(window));
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), 0);
});

add_task(async function pageUpKeyShowsView() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exam",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_PageUp");
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.ok(UrlbarTestUtils.isPopupOpen(window));
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), 0);
});

add_task(async function pageDownKeyWithCtrlKey() {
  const previousTab = gBrowser.selectedTab;
  const currentTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exam",
    fireInputEvent: true,
  });
  EventUtils.synthesizeKey("KEY_PageDown", { ctrlKey: true });
  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(gBrowser.selectedTab, previousTab);
  BrowserTestUtils.removeTab(currentTab);
});

add_task(async function pageUpKeyWithCtrlKey() {
  const previousTab = gBrowser.selectedTab;
  const currentTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exam",
    fireInputEvent: true,
  });
  EventUtils.synthesizeKey("KEY_PageUp", { ctrlKey: true });
  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(gBrowser.selectedTab, previousTab);
  BrowserTestUtils.removeTab(currentTab);
});
