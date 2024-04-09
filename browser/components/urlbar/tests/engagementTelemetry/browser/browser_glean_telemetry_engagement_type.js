/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of engagement telemetry.
// - engagement_type

// This test has many subtests and can time out in verify mode.
requestLongerTimeout(5);

add_setup(async function () {
  await setup();
});

add_task(async function engagement_type_click() {
  await doTest(async () => {
    await openPopup("x");
    await doClick();

    assertEngagementTelemetry([{ engagement_type: "click" }]);
  });
});

add_task(async function engagement_type_enter() {
  await doTest(async () => {
    await openPopup("x");
    await doEnter();

    assertEngagementTelemetry([{ engagement_type: "enter" }]);
  });
});

add_task(async function engagement_type_go_button() {
  await doTest(async () => {
    await openPopup("x");
    EventUtils.synthesizeMouseAtCenter(gURLBar.goButton, {});

    assertEngagementTelemetry([{ engagement_type: "go_button" }]);
  });
});

add_task(async function engagement_type_drop_go() {
  await doTest(async () => {
    await doDropAndGo("example.com");

    assertEngagementTelemetry([{ engagement_type: "drop_go" }]);
  });
});

add_task(async function engagement_type_paste_go() {
  await doTest(async () => {
    await doPasteAndGo("www.example.com");

    assertEngagementTelemetry([{ engagement_type: "paste_go" }]);
  });
});

add_task(async function engagement_type_dismiss() {
  const cleanupQuickSuggest = await ensureQuickSuggestInit();

  await doTest(async () => {
    await openPopup("sponsored");

    const originalResultCount = UrlbarTestUtils.getResultCount(window);
    await selectRowByURL("https://example.com/sponsored");
    UrlbarTestUtils.openResultMenuAndPressAccesskey(window, "D");
    await BrowserTestUtils.waitForCondition(
      () => originalResultCount != UrlbarTestUtils.getResultCount(window)
    );

    assertEngagementTelemetry([{ engagement_type: "dismiss" }]);

    // The view should stay open after dismissing the result. Now pick the
    // heuristic result. Another "click" engagement event should be recorded.
    Assert.ok(
      gURLBar.view.isOpen,
      "View should remain open after dismissing result"
    );
    await doClick();
    assertEngagementTelemetry([
      { engagement_type: "dismiss" },
      { engagement_type: "click", interaction: "typed" },
    ]);
  });

  await doTest(async () => {
    await openPopup("sponsored");

    const originalResultCount = UrlbarTestUtils.getResultCount(window);
    await selectRowByURL("https://example.com/sponsored");
    EventUtils.synthesizeKey("KEY_Delete", { shiftKey: true });
    await BrowserTestUtils.waitForCondition(
      () => originalResultCount != UrlbarTestUtils.getResultCount(window)
    );

    assertEngagementTelemetry([{ engagement_type: "dismiss" }]);
  });

  await cleanupQuickSuggest();
});

add_task(async function engagement_type_help() {
  const url = "https://example.com/";
  const helpUrl = "https://example.com/help";
  let provider = new UrlbarTestUtils.TestProvider({
    priority: Infinity,
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          url,
          isBlockable: true,
          blockL10n: { id: "urlbar-result-menu-dismiss-firefox-suggest" },
          helpUrl,
          helpL10n: {
            id: "urlbar-result-menu-learn-more-about-firefox-suggest",
          },
        }
      ),
    ],
  });
  UrlbarProvidersManager.registerProvider(provider);

  await doTest(async () => {
    await openPopup("test");
    await selectRowByURL(url);

    const onTabOpened = BrowserTestUtils.waitForNewTab(gBrowser);
    UrlbarTestUtils.openResultMenuAndPressAccesskey(window, "L");
    const tab = await onTabOpened;
    BrowserTestUtils.removeTab(tab);

    assertEngagementTelemetry([{ engagement_type: "help" }]);
  });

  UrlbarProvidersManager.unregisterProvider(provider);
});

add_task(async function engagement_type_manage() {
  const cleanupQuickSuggest = await ensureQuickSuggestInit();

  await doTest(async () => {
    await openPopup("sponsored");
    await selectRowByURL("https://example.com/sponsored");

    const onManagePageLoaded = BrowserTestUtils.browserLoaded(
      browser,
      false,
      "about:preferences#search"
    );
    UrlbarTestUtils.openResultMenuAndPressAccesskey(window, "M");
    await onManagePageLoaded;

    assertEngagementTelemetry([{ engagement_type: "manage" }]);
  });

  await cleanupQuickSuggest();
});
