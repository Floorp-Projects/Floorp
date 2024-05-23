/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * The FullPageTranslationsPanel telemetry when transitioning from the unsupported language view.
 */
add_task(async function test_translations_telemetry_unsupported_lang() {
  const { cleanup } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: [
      // Do not include Spanish.
      { fromLang: "fr", toLang: "en" },
      { fromLang: "en", toLang: "fr" },
    ],
    prefs: [["browser.translations.panelShown", false]],
  });

  await FullPageTranslationsTestUtils.openPanel({
    openFromAppMenu: true,
    onOpenPanel:
      FullPageTranslationsTestUtils.assertPanelViewUnsupportedLanguage,
  });

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 1,
    expectNewFlowId: true,
    assertForMostRecentEvent: {
      auto_show: false,
      view_name: "defaultView",
      opened_from: "appMenu",
      document_language: "es",
    },
  });

  await FullPageTranslationsTestUtils.clickChangeSourceLanguageButton({
    firstShow: true,
  });

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.changeSourceLanguageButton,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
    }
  );
  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 1,
    expectNewFlowId: false,
  });
  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 2,
    expectNewFlowId: false,
    assertForMostRecentEvent: {
      auto_show: false,
      view_name: "defaultView",
      opened_from: "appMenu",
      document_language: "es",
    },
  });

  await FullPageTranslationsTestUtils.clickCancelButton();

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.cancelButton,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
    }
  );

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 2,
    expectNewFlowId: false,
  });

  await FullPageTranslationsTestUtils.openPanel({
    openFromAppMenu: true,
    onOpenPanel:
      FullPageTranslationsTestUtils.assertPanelViewUnsupportedLanguage,
  });

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 3,
    expectNewFlowId: true,
    assertForMostRecentEvent: {
      auto_show: false,
      view_name: "defaultView",
      opened_from: "appMenu",
      document_language: "es",
    },
  });

  await FullPageTranslationsTestUtils.clickDismissErrorButton();

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.dismissErrorButton,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
    }
  );
  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 3,
    expectNewFlowId: false,
  });

  await cleanup();
});
