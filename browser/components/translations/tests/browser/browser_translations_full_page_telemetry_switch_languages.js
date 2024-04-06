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
    finalValuePredicates: [
      value => value.extra.auto_show === "false",
      value => value.extra.view_name === "defaultView",
      value => value.extra.opened_from === "translationsButton",
      value => value.extra.document_language === "es",
    ],
  });
  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "en"],
    }
  );

  FullPageTranslationsTestUtils.changeSelectedFromLanguage("es");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 2,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "es"],
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
      finalValuePredicates: [value => value.extra.language === "en"],
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
    finalValuePredicates: [
      value => value.extra.auto_show === "false",
      value => value.extra.view_name === "defaultView",
      value => value.extra.opened_from === "translationsButton",
      value => value.extra.document_language === "es",
    ],
  });
  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "fr"],
    }
  );

  FullPageTranslationsTestUtils.changeSelectedToLanguage("en");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 2,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "en"],
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
      finalValuePredicates: [value => value.extra.language === "en"],
    }
  );

  await cleanup();
});
