/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests interactions with the search mode indicator. See browser_oneOffs.js for
 * more coverage.
 */

const TEST_QUERY = "test string";

add_task(async function setup() {
  SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", false],
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });
});

/**
 * Enters search mode by clicking the first one-off.
 * @param {object} window
 */
async function enterSearchMode(window) {
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(
    window
  ).getSelectableButtons(true);
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(oneOffs[0], {});
  await searchPromise;
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is still open.");
  Assert.ok(
    UrlbarTestUtils.isInSearchMode(window, oneOffs[0].engine.name),
    "The Urlbar is in search mode."
  );
}

// Tests that the indicator is removed when backspacing at the beginning of
// the search string.
add_task(async function backspace() {
  // View open, with string.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await enterSearchMode(window);
  gURLBar.selectionStart = gURLBar.selectionEnd = 0;
  EventUtils.synthesizeKey("KEY_Backspace");
  Assert.equal(gURLBar.value, TEST_QUERY, "Urlbar value hasn't changed.");
  Assert.ok(
    !UrlbarTestUtils.isInSearchMode(window),
    "The Urlbar is no longer in search mode."
  );

  // View open, no string.
  // Open Top Sites.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    if (gURLBar.getAttribute("pageproxystate") == "invalid") {
      gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });
  await enterSearchMode(window);
  EventUtils.synthesizeKey("KEY_Backspace");
  Assert.equal(gURLBar.value, "", "Urlbar value is empty.");
  Assert.ok(
    !UrlbarTestUtils.isInSearchMode(window),
    "The Urlbar is no longer in search mode."
  );

  // View closed, with string.
  // Open Top Sites.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await enterSearchMode(window);
  UrlbarTestUtils.promisePopupClose(window);

  gURLBar.selectionStart = gURLBar.selectionEnd = 0;
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Backspace");
  await searchPromise;
  Assert.equal(gURLBar.value, TEST_QUERY, "Urlbar value hasn't changed.");
  Assert.ok(
    !UrlbarTestUtils.isInSearchMode(window),
    "The Urlbar is no longer in search mode."
  );
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is now open.");

  // View closed, no string.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    if (gURLBar.getAttribute("pageproxystate") == "invalid") {
      gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });
  await enterSearchMode(window);
  UrlbarTestUtils.promisePopupClose(window);

  gURLBar.selectionStart = gURLBar.selectionEnd = 0;
  EventUtils.synthesizeKey("KEY_Backspace");
  Assert.equal(gURLBar.value, "", "Urlbar value is empty.");
  Assert.ok(
    !UrlbarTestUtils.isInSearchMode(window),
    "The Urlbar is no longer in search mode."
  );
  Assert.ok(
    !UrlbarTestUtils.isPopupOpen(window),
    "Urlbar view is still closed."
  );
});

// Tests the indicator's interaction with the ESC key.
add_task(async function escape() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await enterSearchMode(window);

  EventUtils.synthesizeKey("KEY_Escape");
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window, "UrlbarView is closed."));
  Assert.equal(gURLBar.value, TEST_QUERY, "Urlbar value hasn't changed.");
  Assert.ok(
    UrlbarTestUtils.isInSearchMode(window),
    "The Urlbar is in search mode."
  );

  EventUtils.synthesizeKey("KEY_Escape");
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window, "UrlbarView is closed."));
  Assert.ok(!gURLBar.value, "Urlbar value is empty.");
  Assert.ok(
    !UrlbarTestUtils.isInSearchMode(window),
    "The Urlbar is not in search mode."
  );
});

// Tests that the indicator is removed when its close button is clicked.
add_task(async function click_close() {
  // Clicking close with the view open.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await enterSearchMode(window);
  // We need to hover the indicator to make the close button clickable in the
  // test.
  let indicator = gURLBar.querySelector("#urlbar-search-mode-indicator");
  EventUtils.synthesizeMouseAtCenter(indicator, { type: "mouseover" });
  let closeButton = gURLBar.querySelector(
    "#urlbar-search-mode-indicator-close"
  );
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(closeButton, {});
  await searchPromise;
  Assert.ok(
    !UrlbarTestUtils.isInSearchMode(window),
    "The Urlbar is no longer in search mode."
  );

  // Clicking close with the view closed.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await enterSearchMode(window);
  UrlbarTestUtils.promisePopupClose(window);
  if (gURLBar.hasAttribute("breakout-extend")) {
    Assert.ok(
      UrlbarTestUtils.isInSearchMode(window),
      "The Urlbar is still in search mode."
    );
    indicator = gURLBar.querySelector("#urlbar-search-mode-indicator");
    EventUtils.synthesizeMouseAtCenter(indicator, { type: "mouseover" });
    closeButton = gURLBar.querySelector("#urlbar-search-mode-indicator-close");
    EventUtils.synthesizeMouseAtCenter(closeButton, {});
    Assert.ok(
      !UrlbarTestUtils.isInSearchMode(window),
      "The Urlbar is no longer in search mode."
    );
  } else {
    // If the Urlbar is not extended when it is closed, do not finish this
    // case. The close button is not clickable when the Urlbar is not
    // extended. This scenario might be encountered on Linux, where
    // prefers-reduced-motion is enabled in automation.
    gURLBar.setSearchMode(null);
  }
});
