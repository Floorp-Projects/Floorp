/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

/**
 * Back/fwd buttons should be re-enabled after customizing.
 */
add_task(async function test_back_forward_buttons() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.navigation.requireUserInteraction", false]],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PATH);
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    "data:text/html,A separate page"
  );
  await loaded;
  loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    "data:text/html,Another separate page"
  );
  await loaded;
  gBrowser.goBack();
  await BrowserTestUtils.waitForCondition(() => gBrowser.canGoForward);

  let backButton = document.getElementById("back-button");
  let forwardButton = document.getElementById("forward-button");

  await BrowserTestUtils.waitForCondition(
    () =>
      !backButton.hasAttribute("disabled") &&
      !forwardButton.hasAttribute("disabled")
  );

  ok(!backButton.hasAttribute("disabled"), "Back button shouldn't be disabled");
  ok(
    !forwardButton.hasAttribute("disabled"),
    "Forward button shouldn't be disabled"
  );
  await startCustomizing();

  is(
    backButton.getAttribute("disabled"),
    "true",
    "Back button should be disabled in customize mode"
  );
  is(
    forwardButton.getAttribute("disabled"),
    "true",
    "Forward button should be disabled in customize mode"
  );

  await endCustomizing();

  await BrowserTestUtils.waitForCondition(
    () =>
      !backButton.hasAttribute("disabled") &&
      !forwardButton.hasAttribute("disabled")
  );

  ok(
    !backButton.hasAttribute("disabled"),
    "Back button shouldn't be disabled after customize mode"
  );
  ok(
    !forwardButton.hasAttribute("disabled"),
    "Forward button shouldn't be disabled after customize mode"
  );

  BrowserTestUtils.removeTab(tab);
});
