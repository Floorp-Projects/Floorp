/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

/**
 * Ensure that click handlers that prevent the default action but fire
 * a different click event that does open a tab are allowed to work.
 */
add_task(async function test_untrusted_click_opens_tab() {
  await BrowserTestUtils.withNewTab(TEST_PATH + "click.html", async browser => {
    let newTabOpened = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "https://example.org/",
      true
    );
    info("clicking link with modifier pressed.");
    let eventObj = {};
    eventObj[AppConstants.platform == "macosx" ? "metaKey" : "ctrlKey"] = true;
    await BrowserTestUtils.synthesizeMouseAtCenter("#test", eventObj, browser);
    info(
      "Waiting for new tab to open from frontend; if we timeout the test is broken."
    );
    let newTab = await newTabOpened;
    ok(newTab, "New tab should be opened.");
    BrowserTestUtils.removeTab(newTab);
  });
});

/**
 * Ensure that click handlers that redirect a modifier-less click into
 * an untrusted event without modifiers don't run afoul of the popup
 * blocker by having their transient user gesture bit consumed in the
 * child by the handling code for modified clicks.
 */
add_task(async function test_unused_click_doesnt_consume_activation() {
  // Enable the popup blocker.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.disable_open_during_load", true],
      ["dom.block_multiple_popups", true],
    ],
  });
  await BrowserTestUtils.withNewTab(TEST_PATH + "click.html", async browser => {
    let newTabOpened = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "https://example.com/",
      true
    );
    info("clicking button that generates link press.");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#fire-untrusted",
      {},
      browser
    );
    info(
      "Waiting for new tab to open through gecko; if we timeout the test is broken."
    );
    let newTab = await newTabOpened;
    ok(newTab, "New tab should be opened.");
    BrowserTestUtils.removeTab(newTab);
  });
});

/**
 * Ensure that click handlers redirecting to elements without href don't
 * trigger user gesture consumption either.
 */
add_task(async function test_click_without_href_doesnt_consume_activation() {
  // Enable the popup blocker.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.disable_open_during_load", true],
      ["dom.block_multiple_popups", true],
    ],
  });
  await BrowserTestUtils.withNewTab(TEST_PATH + "click.html", async browser => {
    let newTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:blank");
    info("clicking link that generates link click on target-less link.");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#fire-targetless-link",
      {},
      browser
    );
    info(
      "Waiting for new tab to open after targetless link; if we timeout the test is broken."
    );
    let newTab = await newTabOpened;
    ok(newTab, "New tab should be opened.");
    BrowserTestUtils.removeTab(newTab);

    newTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:blank");
    info("clicking link that generates button click.");
    await BrowserTestUtils.synthesizeMouseAtCenter("#fire-button", {}, browser);
    info(
      "Waiting for new tab to open after button redirect; if we timeout the test is broken."
    );
    newTab = await newTabOpened;
    ok(newTab, "New tab should be opened.");
    BrowserTestUtils.removeTab(newTab);
  });
});
