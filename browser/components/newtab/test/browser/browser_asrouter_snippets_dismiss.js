/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Snippets endpoint has two snippets that share the same campaign id.
 * We want to make sure that dismissing the snippet on the first about:newtab
 * will clear the snippet on the next (preloaded) about:newtab.
 */

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.providers.snippets",
        '{"id":"snippets","enabled":true,"type":"remote","url":"https://example.com/browser/browser/components/newtab/test/browser/snippet.json","updateCycleInMs":14400000}',
      ],
      ["browser.newtabpage.activity-stream.feeds.snippets", true],
      // Disable onboarding, this would prevent snippets from showing up
      [
        "browser.newtabpage.activity-stream.asrouter.providers.onboarding",
        '{"id":"onboarding","type":"local","localProvider":"OnboardingMessageProvider","enabled":false,"exclude":[]}',
      ],
      // Ensure this is true, this is the main behavior we want to test for
      ["browser.newtab.preload", true],
    ],
  });
}

add_task(async function test_campaign_dismiss() {
  await setup();
  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:home",
  });
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async () => {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".activity-stream"),
      `Should render Activity Stream`
    );
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector(
          "#footer-asrouter-container .SimpleSnippet"
        ),
      "Should find the snippet inside the footer container"
    );

    content.document
      .querySelector("#footer-asrouter-container .blockButton")
      .click();

    await ContentTaskUtils.waitForCondition(
      () =>
        !content.document.querySelector(
          "#footer-asrouter-container .SimpleSnippet"
        ),
      "Should wait for the snippet to block"
    );
  });

  ok(
    ASRouter.state.messageBlockList.length,
    "Should have the campaign blocked"
  );

  let tab2 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:newtab",
    // This is important because the newtab is preloaded and doesn't behave
    // like a regular page load
    waitForLoad: false,
  });

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async () => {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".activity-stream"),
      `Should render Activity Stream`
    );
    let snippet = content.document.querySelector(
      "#footer-asrouter-container .SimpleSnippet"
    );
    Assert.equal(
      snippet,
      null,
      "No snippets shown because campaign is blocked"
    );
  });

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  await ASRouter.unblockMessageById(["10533", "10534"]);
  await SpecialPowers.popPrefEnv();
});
