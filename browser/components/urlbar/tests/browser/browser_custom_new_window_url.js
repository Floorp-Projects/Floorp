/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures that when a custom new window URL is configured and a new
 * window is opened, the URL bar is focused and the URL string is selected.
 *
 * See bug 1770818 for additional details.
 */

"use strict";

const CUSTOM_URL = "http://example.com/";
const DEFAULT_URL = HomePage.getOriginalDefault();

add_setup(async function() {
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

add_task(async function checkFocusAndSelectionWithCustomURL() {
  function urlTextSelectedPredicateFunc() {
    return (
      newWin.gURLBar.selectionStart == 0 &&
      newWin.gURLBar.selectionEnd == newWin.gURLBar.value.length &&
      newWin.gURLBar.selectionStart != newWin.gURLBar.selectionEnd
    );
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.startup.homepage", CUSTOM_URL],
      ["browser.startup.page", 1],
    ],
  });

  let newWinPromise = BrowserTestUtils.waitForNewWindow({
    url: CUSTOM_URL,
  });

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  await newWinPromise;

  let isURLTextSelected = await TestUtils.waitForCondition(
    urlTextSelectedPredicateFunc,
    "The text inside the URL bar should be selected"
  );

  Assert.ok(
    newWin.gURLBar.focused,
    "URL bar is focused upon opening a new window"
  );
  Assert.ok(
    isURLTextSelected,
    "URL bar text is selected upon opening a new window"
  );

  await BrowserTestUtils.closeWindow(newWin);
  await SpecialPowers.popPrefEnv();
});

add_task(async function checkFocusWithDefaultURL() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.startup.homepage", DEFAULT_URL],
      ["browser.startup.page", 1],
    ],
  });

  let newWinPromise = BrowserTestUtils.waitForNewWindow({
    url: DEFAULT_URL,
  });
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  await newWinPromise;

  Assert.ok(
    newWin.gURLBar.focused,
    "URL bar is focused upon opening a new window"
  );
  Assert.equal(newWin.gURLBar.value, "", "URL bar should be empty");

  await BrowserTestUtils.closeWindow(newWin);
  await SpecialPowers.popPrefEnv();
});
