/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);

/**
 * Tests that the first-interaction event status is maintained across a subsequent panel
 * open, if re-opening the panel is due to a translation failure.
 */
add_task(async function test_translations_telemetry_firstrun_failure() {
  const { cleanup, rejectDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.panelShown", false]],
  });

  await FullPageTranslationsTestUtils.assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The button is available."
  );

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await FullPageTranslationsTestUtils.openTranslationsPanel({
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewFirstShow,
  });

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

  await FullPageTranslationsTestUtils.clickTranslateButton({
    downloadHandler: rejectDownloads,
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewFirstShowError,
  });

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 2,
    expectNewFlowId: false,
    expectFirstInteraction: true,
    finalValuePredicates: [
      value => value.extra.auto_show === "true",
      value => value.extra.view_name === "errorView",
      value => value.extra.opened_from === "translationsButton",
      value => value.extra.document_language === "es",
    ],
  });
  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.translateButton,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      expectFirstInteraction: true,
    }
  );
  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 1,
    expectNewFlowId: false,
    expectFirstInteraction: true,
  });
  await TestTranslationsTelemetry.assertEvent(Glean.translations.error, {
    expectedEventCount: 1,
    expectNewFlowId: false,
    expectFirstInteraction: true,
    finalValuePredicates: [
      value =>
        value.extra.reason === "Error: Intentionally rejecting downloads.",
    ],
  });
  await TestTranslationsTelemetry.assertEvent(
    Glean.translations.translationRequest,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      expectFirstInteraction: true,
      finalValuePredicates: [
        value => value.extra.from_language === "es",
        value => value.extra.to_language === "en",
        value => value.extra.auto_translate === "false",
        value => value.extra.document_language === "es",
        value => value.extra.top_preferred_language === "en",
      ],
    }
  );

  await FullPageTranslationsTestUtils.clickCancelButton();

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.cancelButton,
    {
      expectedEventCount: 1,
      expectNewFlowId: false,
      expectFirstInteraction: true,
    }
  );
  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 2,
    expectNewFlowId: false,
    expectFirstInteraction: true,
  });

  await FullPageTranslationsTestUtils.openTranslationsPanel({
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewFirstShow,
  });

  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.open, {
    expectedEventCount: 3,
    expectNewFlowId: true,
    expectFirstInteraction: false,
    finalValuePredicates: [
      value => value.extra.auto_show === "false",
      value => value.extra.view_name === "defaultView",
      value => value.extra.opened_from === "translationsButton",
      value => value.extra.document_language === "es",
    ],
  });

  await FullPageTranslationsTestUtils.clickCancelButton();

  await TestTranslationsTelemetry.assertEvent(
    Glean.translationsPanel.cancelButton,
    {
      expectedEventCount: 2,
      expectNewFlowId: false,
      expectFirstInteraction: false,
    }
  );
  await TestTranslationsTelemetry.assertEvent(Glean.translationsPanel.close, {
    expectedEventCount: 3,
    expectNewFlowId: false,
    expectFirstInteraction: false,
  });

  await cleanup();
});
