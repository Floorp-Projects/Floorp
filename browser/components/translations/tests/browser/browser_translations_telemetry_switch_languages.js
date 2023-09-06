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

  await assertTranslationsButton({ button: true }, "The button is available.");

  await assertPageIsUntranslated(runInPage);

  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });

  assertSelectedFromLanguage("es");
  switchSelectedFromLanguage("en");

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

  switchSelectedFromLanguage("es");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 2,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "es"],
    }
  );

  switchSelectedFromLanguage("");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeFromLanguage,
    {
      expectedEventCount: 2,
    }
  );

  switchSelectedFromLanguage("en");

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

  await assertTranslationsButton({ button: true }, "The button is available.");

  await assertPageIsUntranslated(runInPage);

  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });

  assertSelectedToLanguage("en");
  switchSelectedToLanguage("fr");

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

  switchSelectedToLanguage("en");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 2,
      expectNewFlowId: false,
      finalValuePredicates: [value => value.extra.language === "en"],
    }
  );

  switchSelectedToLanguage("");

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeToLanguage,
    {
      expectedEventCount: 2,
    }
  );

  switchSelectedToLanguage("en");

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
