/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the telemetry event for a manual translation request.
 */
add_task(async function test_translations_telemetry_manual_translation() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  const { button } = await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
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

  await TestTranslationsTelemetry.assertCounter(
    "RequestCount",
    Glean.translations.requestsCount,
    0
  );
  await TestTranslationsTelemetry.assertRate(
    "ErrorRate",
    Glean.translations.errorRate,
    {
      expectedNumerator: 0,
      expectedDenominator: 0,
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "TranslationRequest",
    Glean.translations.translationRequest,
    {
      expectedEventCount: 0,
    }
  );

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(button, "Opening the popup");
    },
    assertPanelDefaultView
  );

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-translate-button"),
      "Start translating by clicking the translate button."
    );
  });

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
      "The pages H1 is translated.",
      getH1,
      "DON QUIJOTE DE LA MANCHA [es to en, html]"
    );
  });

  await TestTranslationsTelemetry.assertCounter(
    "RequestCount",
    Glean.translations.requestsCount,
    1
  );
  await TestTranslationsTelemetry.assertRate(
    "ErrorRate",
    Glean.translations.errorRate,
    {
      expectedNumerator: 0,
      expectedDenominator: 1,
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedEventCount: 1,
      expectNewFlowId: true,
      finalValuePredicates: [
        value => value.extra.auto_show === "false",
        value => value.extra.view_name === "defaultView",
        value => value.extra.opened_from === "translationsButton",
        value => value.extra.document_language === "es",
      ],
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "TranslateButton",
    Glean.translationsPanel.translateButton,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "ClosePanel",
    Glean.translationsPanel.close,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "TranslationRequest",
    Glean.translations.translationRequest,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      finalValuePredicates: [
        value => value.extra.from_language === "es",
        value => value.extra.to_language === "en",
        value => value.extra.auto_translate === "false",
        value => value.extra.document_language === "es",
        value => value.extra.top_preferred_language === "en",
      ],
    }
  );

  await cleanup();
});

/**
 * Tests the telemetry event for an automatic translation request.
 */
add_task(async function test_translations_telemetry_auto_translation() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.alwaysTranslateLanguages", "es"]],
  });

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
      "The pages H1 is translated.",
      getH1,
      "DON QUIJOTE DE LA MANCHA [es to en, html]"
    );
  });

  await TestTranslationsTelemetry.assertCounter(
    "RequestCount",
    Glean.translations.requestsCount,
    1
  );
  await TestTranslationsTelemetry.assertRate(
    "ErrorRate",
    Glean.translations.errorRate,
    {
      expectedNumerator: 0,
      expectedDenominator: 1,
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "OpenPanel",
    Glean.translationsPanel.open,
    {
      expectedEventCount: 0,
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "TranslateButton",
    Glean.translationsPanel.translateButton,
    {
      expectedEventCount: 0,
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "ClosePanel",
    Glean.translationsPanel.close,
    {
      expectedEventCount: 0,
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "TranslationRequest",
    Glean.translations.translationRequest,
    {
      expectedEventCount: 1,
      expectNewFlowId: true,
      finalValuePredicates: [
        value => value.extra.from_language === "es",
        value => value.extra.to_language === "en",
        value => value.extra.auto_translate === "true",
        value => value.extra.document_language === "es",
        value => value.extra.top_preferred_language === "en",
      ],
    }
  );

  await cleanup();
});
