/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const {
  openFirefoxViewTab,
  withFirefoxView,
  init: FirefoxViewTestUtilsInit,
} = ChromeUtils.importESModule(
  "resource://testing-common/FirefoxViewTestUtils.sys.mjs"
);

/**
 * Bug 1856572 - This test is to ensure that fluent strings in Firefox View
 * can be updated after opening Customize Mode
 * **/
add_task(async function test_data_l10n_customize_mode() {
  FirefoxViewTestUtilsInit(this);
  await withFirefoxView({ win: window }, async function (browser) {
    /**
     * Bug 1856572, Bug 1857622: Without requesting two animation frames
     * the "missing Fluent strings" issue will not reproduce.
     *
     * Given the precondition that we open Firefox View, then open Customize Mode, then
     * navigate back to Firefox View in a quick succession, as long as Fluent
     * controlled strings can be updated by changing their Fluent IDs then this
     * test is valid.
     */
    await new Promise(r =>
      requestAnimationFrame(() => requestAnimationFrame(r))
    );
    await startCustomizing();
    await endCustomizing();
    await openFirefoxViewTab(window);
    const { document } = browser.contentWindow;
    let secondPageNavButton = document.querySelectorAll(
      "moz-page-nav-button"
    )[0];
    document.l10n.setAttributes(
      secondPageNavButton,
      "firefoxview-overview-header"
    );
    let previousText = await document.l10n.formatValue(
      "firefoxview-opentabs-nav"
    );
    let translatedText = await window.content.document.l10n.formatValue(
      "firefoxview-overview-header"
    );
    /**
     * This should be replaced with
     * BrowserTestUtils.waitForMutationCondition(secondPageNavButton, {characterData: true}, ...)
     * but apparently Fluent manipulation of textContent doesn't result
     * in a characterData mutation occurring.
     */
    await BrowserTestUtils.waitForCondition(() => {
      return (
        secondPageNavButton.textContent !== previousText &&
        secondPageNavButton.textContent === translatedText
      );
    }, "waiting for text content to change");

    Assert.equal(
      secondPageNavButton.getAttribute("data-l10n-id"),
      "firefoxview-overview-header",
      "data-l10n-id should be updated"
    );
    Assert.notEqual(
      previousText,
      secondPageNavButton.textContent,
      "The second page-nav button text content should be updated"
    );

    Assert.equal(
      translatedText,
      secondPageNavButton.textContent,
      "The changed text should be the translated value of 'firefoxview-overview-header"
    );
  });
});
