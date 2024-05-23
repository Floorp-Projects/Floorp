/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the telemetry events for switching the from-language.
 */
add_task(async function test_translations_telemetry_switch_from_language() {
  const { cleanup, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  await FullPageTranslationsTestUtils.assertTranslationsButton(
    { button: true },
    "The button is available."
  );

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await FullPageTranslationsTestUtils.openPanel({
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
  });

  FullPageTranslationsTestUtils.assertSelectedFromLanguage({ langTag: "es" });
  FullPageTranslationsTestUtils.changeSelectedFromLanguage("en");

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 1,
    expectNewFlowId: true,
    assertForMostRecentEvent: {
      auto_show: false,
      view_name: "defaultView",
      opened_from: "translationsButton",
      document_language: "es",
    },
  });
  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      assertForMostRecentEvent: {
        language: "en",
      },
    }
  );

  FullPageTranslationsTestUtils.changeSelectedFromLanguage("es");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 2,
      expectNewFlowId: false,
      assertForMostRecentEvent: {
        language: "es",
      },
    }
  );

  FullPageTranslationsTestUtils.changeSelectedFromLanguage("");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 2,
    }
  );

  FullPageTranslationsTestUtils.changeSelectedFromLanguage("en");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 3,
      expectNewFlowId: false,
      assertForMostRecentEvent: {
        language: "en",
      },
    }
  );

  await cleanup();
});

/**
 * Tests the telemetry events for switching the to-language.
 */
add_task(async function test_translations_telemetry_switch_to_language() {
  const { cleanup, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  await FullPageTranslationsTestUtils.assertTranslationsButton(
    { button: true },
    "The button is available."
  );

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await FullPageTranslationsTestUtils.openPanel({
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
  });

  FullPageTranslationsTestUtils.assertSelectedToLanguage({ langTag: "en" });
  FullPageTranslationsTestUtils.changeSelectedToLanguage("fr");

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 1,
    expectNewFlowId: true,
    assertForMostRecentEvent: {
      auto_show: false,
      view_name: "defaultView",
      opened_from: "translationsButton",
      document_language: "es",
    },
  });
  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      assertForMostRecentEvent: {
        language: "fr",
      },
    }
  );

  FullPageTranslationsTestUtils.changeSelectedToLanguage("en");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 2,
      expectNewFlowId: false,
      assertForMostRecentEvent: {
        language: "en",
      },
    }
  );

  FullPageTranslationsTestUtils.changeSelectedToLanguage("");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 2,
    }
  );

  FullPageTranslationsTestUtils.changeSelectedToLanguage("en");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 3,
      expectNewFlowId: false,
      assertForMostRecentEvent: {
        language: "en",
      },
    }
  );

  await cleanup();
});
