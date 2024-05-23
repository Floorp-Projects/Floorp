/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests basic usage of the Translations panel, and that the telemetry events are logged accordingly
 */
add_task(async function test_translations_telemetry_basics() {
  const { cleanup } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.panelShown", false]],
  });

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 0,
  });

  await FullPageTranslationsTestUtils.assertTranslationsButton(
    { button: true },
    "The button is available."
  );

  await FullPageTranslationsTestUtils.openPanel({
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewFirstShow,
  });

  await FullPageTranslationsTestUtils.clickCancelButton();

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 1,
    expectNewFlowId: true,
    finalValuePredicates: [
      value => value.extra.auto_show === "false",
      value => value.extra.view_name === "defaultView",
      value => value.extra.opened_from === "translationsButton",
      value => value.extra.document_language === "es",
    ],
  });

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.cancelButton,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
    }
  );

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 1,
    expectNewFlowId: false,
  });

  await FullPageTranslationsTestUtils.openPanel({
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewFirstShow,
  });

  await FullPageTranslationsTestUtils.clickCancelButton();

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 2,
    expectNewFlowId: true,
    allValuePredicates: [
      value => value.extra.auto_show === "false",
      value => value.extra.view_name === "defaultView",
      value => value.extra.opened_from === "translationsButton",
      value => value.extra.document_language === "es",
    ],
  });

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.cancelButton,
    {
      expectedEventCount: 2,
      expectNewFlowId: false,
    }
  );

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 2,
    expectNewFlowId: false,
  });

  await cleanup();
});
