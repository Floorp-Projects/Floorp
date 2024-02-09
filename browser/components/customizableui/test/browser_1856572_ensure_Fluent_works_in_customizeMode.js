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

add_task(async function test_data_l10n_customize_mode() {
  FirefoxViewTestUtilsInit(this);
  await withFirefoxView({ win: window }, async function (browser) {
    /**
     * Bug 1856572, Bug 1857622: Without requesting two animation frames
     * the "missing Fluent strings" issue will not reproduce.
     */
    await new Promise(r =>
      requestAnimationFrame(() => requestAnimationFrame(r))
    );
    await startCustomizing();
    await endCustomizing();
    await openFirefoxViewTab(window);

    const { document } = browser.contentWindow;
    let header = document.querySelector("h1");
    document.l10n.setAttributes(header, "firefoxview-overview-header");
    let previousText = await document.l10n.formatValue(
      "firefoxview-page-title"
    );
    /**
     * This should be replaced with
     * BrowserTestUtils.waitForMutationCondition(header, {characterData: true}, ...)
     * but apparently Fluent manipulation of textContent doesn't result
     * in a characterData mutation occurring.
     */
    await BrowserTestUtils.waitForCondition(() => {
      return header.textContent != previousText;
    }, "waiting for text content to change");

    Assert.equal(
      header.getAttribute("data-l10n-id"),
      "firefoxview-overview-header",
      "data-l10n-id should be updated"
    );
    Assert.notEqual(
      previousText,
      header.textContent,
      "The header's text content should be updated"
    );
    let translatedText = await window.content.document.l10n.formatValue(
      "firefoxview-overview-header"
    );
    Assert.equal(
      translatedText,
      header.textContent,
      "The changed text should be the translated value of 'firefoxview-overview-header"
    );
  });
});
