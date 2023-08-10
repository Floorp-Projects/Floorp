/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);

/**
 * Tests the telemetry event for a manual translation request failure.
 */
add_task(
  async function test_translations_telemetry_manual_translation_failure() {
    PromiseTestUtils.expectUncaughtRejection(
      /Intentionally rejecting downloads./
    );

    const { cleanup, rejectDownloads, runInPage } = await loadTestPage({
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

    await rejectDownloads(1);

    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is in Spanish.",
        getH1,
        "Don Quijote de La Mancha"
      );
    });

    assertPanelErrorView();
    await TestTranslationsTelemetry.assertEvent(
      "OpenPanel",
      Glean.translationsPanel.open,
      {
        expectedEventCount: 2,
        expectNewFlowId: false,
        finalValuePredicates: [
          value => value.extra.auto_show === "true",
          value => value.extra.view_name === "errorView",
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
    await TestTranslationsTelemetry.assertCounter(
      "RequestCount",
      Glean.translations.requestsCount,
      1
    );
    await TestTranslationsTelemetry.assertRate(
      "ErrorRate",
      Glean.translations.errorRate,
      {
        expectedNumerator: 1,
        expectedDenominator: 1,
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      "Error",
      Glean.translations.error,
      {
        expectedEventCount: 1,
        expectNewFlowId: false,
        finalValuePredicates: [
          value =>
            value.extra.reason === "Error: Intentionally rejecting downloads.",
        ],
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
  }
);

/**
 * Tests the telemetry event for an automatic translation request failure.
 */
add_task(async function test_translations_telemetry_auto_translation_failure() {
  PromiseTestUtils.expectUncaughtRejection(
    /Intentionally rejecting downloads./
  );

  const { cleanup, rejectDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.alwaysTranslateLanguages", "es"]],
  });

  await assertTranslationsButton(
    { button: true },
    "The translations button is available."
  );

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  await assertTranslationsButton(
    { button: true, circleArrows: true, locale: false, icon: true },
    "The icon presents the loading indicator."
  );

  await rejectDownloads(1);

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  assertPanelErrorView();
  await TestTranslationsTelemetry.assertCounter(
    "RequestCount",
    Glean.translations.requestsCount,
    1
  );
  await TestTranslationsTelemetry.assertRate(
    "ErrorRate",
    Glean.translations.errorRate,
    {
      expectedNumerator: 1,
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
        value => value.extra.auto_show === "true",
        value => value.extra.view_name === "errorView",
        value => value.extra.opened_from === "translationsButton",
        value => value.extra.document_language === "es",
      ],
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "ClosePanel",
    Glean.translationsPanel.close,
    {
      expectedEventCount: 0,
      expectNewFlowId: false,
    }
  );
  await TestTranslationsTelemetry.assertEvent(
    "Error",
    Glean.translations.error,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      finalValuePredicates: [
        value =>
          value.extra.reason === "Error: Intentionally rejecting downloads.",
      ],
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
        value => value.extra.auto_translate === "true",
        value => value.extra.document_language === "es",
        value => value.extra.top_preferred_language === "en",
      ],
    }
  );

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-translate-cancel"),
      "Click the cancel button."
    );
  });
  await TestTranslationsTelemetry.assertEvent(
    "CancelButton",
    Glean.translationsPanel.cancelButton,
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

  await cleanup();
});
