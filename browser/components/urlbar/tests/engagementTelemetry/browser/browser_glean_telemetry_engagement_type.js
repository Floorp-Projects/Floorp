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
  await doTest(async browser => {
    await openPopup("x");
    await doClick();

    assertEngagementTelemetry([{ engagement_type: "click" }]);
  });
});

add_task(async function engagement_type_enter() {
  await doTest(async browser => {
    await openPopup("x");
    await doEnter();

    assertEngagementTelemetry([{ engagement_type: "enter" }]);
  });
});

add_task(async function engagement_type_go_button() {
  await doTest(async browser => {
    await openPopup("x");
    // We intentionally change this a11y check, because the following click is
    // sent to test the behavior of a purposefully non-focusable image button
    // using an alternative way of the urlbar search query submission, where
    // other ways are keyboard accessible (and are tested above).
    AccessibilityUtils.setEnv({
      focusableRule: false,
    });
    EventUtils.synthesizeMouseAtCenter(gURLBar.goButton, {});
    AccessibilityUtils.resetEnv();

    assertEngagementTelemetry([{ engagement_type: "go_button" }]);
  });
});

add_task(async function engagement_type_drop_go() {
  await doTest(async browser => {
    await doDropAndGo("example.com");

    assertEngagementTelemetry([{ engagement_type: "drop_go" }]);
  });
});

add_task(async function engagement_type_paste_go() {
  await doTest(async browser => {
    await doPasteAndGo("www.example.com");

    assertEngagementTelemetry([{ engagement_type: "paste_go" }]);
  });
});

add_task(async function engagement_type_dismiss() {
  const cleanupQuickSuggest = await ensureQuickSuggestInit();

  await doTest(async browser => {
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

  await doTest(async browser => {
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
  const cleanupQuickSuggest = await ensureQuickSuggestInit();

  await doTest(async browser => {
    await openPopup("sponsored");
    await selectRowByURL("https://example.com/sponsored");
    const onTabOpened = BrowserTestUtils.waitForNewTab(gBrowser);
    UrlbarTestUtils.openResultMenuAndPressAccesskey(window, "L");
    const tab = await onTabOpened;
    BrowserTestUtils.removeTab(tab);

    assertEngagementTelemetry([{ engagement_type: "help" }]);
  });

  await cleanupQuickSuggest();
});
