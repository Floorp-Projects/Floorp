/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test case verifies the counts and extra data sent from telemetry events when interacting
 * with the SelectTranslationsPanel's initialization-failure UI states.
 */
add_task(
  async function test_select_translations_panel_telemetry_init_failure_ui() {
    const { cleanup, runInPage, resolveDownloads } = await loadTestPage({
      page: SELECT_TEST_PAGE_URL,
      LANGUAGE_PAIRS,
      prefs: [["browser.translations.select.enable", true]],
    });

    TranslationsPanelShared.simulateLangListError();
    await SelectTranslationsTestUtils.openPanel(runInPage, {
      selectSpanishSection: true,
      openAtSpanishSection: true,
      onOpenPanel: SelectTranslationsTestUtils.assertPanelViewInitFailure,
    });
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.open,
      {
        expectedEventCount: 1,
        expectNewFlowId: true,
        finalValuePredicates: [
          value => value.extra.document_language === "es",
          value => value.extra.from_language === "es",
          value => value.extra.to_language === "en",
          value => value.extra.top_preferred_language === "en",
        ],
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.initializationFailureMessage,
      {
        expectedEventCount: 1,
      }
    );

    TranslationsPanelShared.simulateLangListError();
    await SelectTranslationsTestUtils.waitForPanelPopupEvent(
      "popupshown",
      SelectTranslationsTestUtils.clickTryAgainButton,
      SelectTranslationsTestUtils.assertPanelViewInitFailure
    );
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.tryAgainButton,
      {
        expectedEventCount: 1,
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.close,
      {
        expectedEventCount: 1,
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.open,
      {
        expectedEventCount: 2,
        expectNewFlowId: false,
        finalValuePredicates: [
          value => value.extra.document_language === "es",
          value => value.extra.from_language === "es",
          value => value.extra.to_language === "en",
          value => value.extra.top_preferred_language === "en",
        ],
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.initializationFailureMessage,
      {
        expectedEventCount: 2,
      }
    );

    await SelectTranslationsTestUtils.clickCancelButton();
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.cancelButton,
      {
        expectedEventCount: 1,
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.close,
      {
        expectedEventCount: 2,
      }
    );

    TranslationsPanelShared.simulateLangListError();
    await SelectTranslationsTestUtils.openPanel(runInPage, {
      selectSpanishSection: true,
      openAtSpanishSection: true,
      onOpenPanel: SelectTranslationsTestUtils.assertPanelViewInitFailure,
    });
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.open,
      {
        expectedEventCount: 3,
        expectNewFlowId: true,
        finalValuePredicates: [
          value => value.extra.document_language === "es",
          value => value.extra.from_language === "es",
          value => value.extra.to_language === "en",
          value => value.extra.top_preferred_language === "en",
        ],
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.initializationFailureMessage,
      {
        expectedEventCount: 3,
      }
    );

    await SelectTranslationsTestUtils.clickTryAgainButton({
      downloadHandler: resolveDownloads,
      viewAssertion: SelectTranslationsTestUtils.assertPanelViewTranslated,
    });
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.tryAgainButton,
      {
        expectedEventCount: 2,
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.close,
      {
        expectedEventCount: 3,
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.open,
      {
        expectedEventCount: 4,
        expectNewFlowId: false,
        finalValuePredicates: [
          value => value.extra.document_language === "es",
          value => value.extra.from_language === "es",
          value => value.extra.to_language === "en",
          value => value.extra.top_preferred_language === "en",
        ],
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      Glean.translations.translationRequest,
      {
        expectedEventCount: 1,
        finalValuePredicates: [
          value => value.extra.document_language === "es",
          value => value.extra.from_language === "es",
          value => value.extra.to_language === "en",
          value => value.extra.top_preferred_language === "en",
          value => value.extra.request_target === "select",
          value => value.extra.auto_translate === "false",
          value =>
            value.extra.source_text_code_units ===
            (AppConstants.platform === "win"
              ? "2064" // With carriage returns
              : "2041"), // No carriage returns
          value => value.extra.source_text_word_count === "358",
        ],
      }
    );

    await SelectTranslationsTestUtils.clickDoneButton();
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.doneButton,
      {
        expectedEventCount: 1,
      }
    );
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.close,
      {
        expectedEventCount: 4,
      }
    );

    await cleanup();
  }
);
