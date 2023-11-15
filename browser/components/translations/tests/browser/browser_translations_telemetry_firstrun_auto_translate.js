/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the entire flow of opening the translation settings menu and initiating
 * an auto-translate request on the first panel interaction.
 */
add_task(async function test_translations_telemetry_firstrun_auto_translate() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.panelShown", false]],
  });

  await assertTranslationsButton({ button: true }, "The button is available.");

  await assertPageIsUntranslated(runInPage);

  await openTranslationsPanel({ onOpenPanel: assertPanelFirstShowView });
  await openTranslationsSettingsMenu();
  await clickAlwaysTranslateLanguage({
    downloadHandler: resolveDownloads,
  });

  await assertPageIsTranslated("es", "en", runInPage);

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 1,
    expectNewFlowId: true,
    expectFirstInteraction: true,
    finalValuePredicates: [
      value => value.extra.auto_show === "false",
      value => value.extra.view_name === "defaultView",
      value => value.extra.opened_from === "translationsButton",
      value => value.extra.document_language === "es",
    ],
  });
  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.alwaysTranslateLanguage,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      expectFirstInteraction: true,
      finalValuePredicates: [value => value.extra.language === "es"],
    }
  );
  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 1,
    expectNewFlowId: false,
    expectFirstInteraction: true,
  });
  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 1,
    expectNewFlowId: false,
    expectFirstInteraction: true,
  });
  await TestTranslationsTelemetry.assertEvent(
    Glean.translations.translationRequest,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      expectFirstInteraction: true,
    }
  );

  await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });

  await clickRestoreButton();

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 2,
    expectNewFlowId: true,
    expectFirstInteraction: false,
    finalValuePredicates: [
      value => value.extra.auto_show === "false",
      value => value.extra.view_name === "revisitView",
      value => value.extra.opened_from === "translationsButton",
      value => value.extra.document_language === "es",
    ],
  });

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.restorePageButton,
    {
      expectedEventCount: 1,
      expectFirstInteraction: false,
      expectNewFlowId: false,
    }
  );

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 2,
    expectFirstInteraction: false,
    expectNewFlowId: false,
  });

  await assertTranslationsButton({ button: true }, "The button is available.");
  await assertPageIsUntranslated(runInPage);

  await cleanup();
});
