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
    prefs: [
      ["browser.translations.panelShown", false],
      ["browser.translations.alwaysTranslateLanguages", ""],
    ],
  });

  const { button } = await assertTranslationsButton(
    { button: true },
    "The button is available."
  );

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  await openTranslationsSettingsMenuViaTranslationsButton();
  await toggleAlwaysTranslateLanguage();

  await assertTranslationsButton(
    { button: true, circleArrows: true, locale: false, icon: true },
    "The icon presents the loading indicator."
  );

  await resolveDownloads(1);

  const { locale } = await assertTranslationsButton(
    { button: true, circleArrows: false, locale: true, icon: true },
    "The icon presents the locale."
  );

  is(locale.innerText, "en", "The English language tag is shown.");

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is translated automatically",
      getH1,
      "DON QUIJOTE DE LA MANCHA [es to en, html]"
    );
  });

  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedEventCount: 1,
      expectNewFlowId: true,
      expectFirstInteraction: true,
      finalValuePredicates: [
        value => value.extra.auto_show === "false",
        value => value.extra.view_name === "defaultView",
        value => value.extra.opened_from === "translationsButton",
        value => value.extra.document_language === "es",
      ],
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "alwaysTranslateLanguage",
    Glean.translationsPanel.alwaysTranslateLanguage,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      expectFirstInteraction: true,
      finalValuePredicates: [value => value.extra.language === "es"],
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "ClosePanel",
    Glean.translationsPanel.close,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      expectFirstInteraction: true,
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "ClosePanel",
    Glean.translationsPanel.close,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      expectFirstInteraction: true,
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "TranslationRequest",
    Glean.translations.translationRequest,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      expectFirstInteraction: true,
    }
  );

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(button, "Opening the popup");
    },
    assertPanelRevisitView
  );

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-restore-button"),
      "Click the restore-page button."
    );
  });

  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedEventCount: 2,
      expectNewFlowId: true,
      expectFirstInteraction: false,
      finalValuePredicates: [
        value => value.extra.auto_show === "false",
        value => value.extra.view_name === "revisitView",
        value => value.extra.opened_from === "translationsButton",
        value => value.extra.document_language === "es",
      ],
    }
  );

  await TestTranslationsTelemetry.assertEvent(
    "RestorePageButton",
    Glean.translationsPanel.restorePageButton,
    {
      expectedEventCount: 1,
      expectFirstInteraction: false,
      expectNewFlowId: false,
    }
  );

  await TestTranslationsTelemetry.assertEvent(
    "ClosePanel",
    Glean.translationsPanel.close,
    {
      expectedEventCount: 2,
      expectFirstInteraction: false,
      expectNewFlowId: false,
    }
  );

  await assertTranslationsButton({ button: true }, "The button is available.");

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  await cleanup();
});
