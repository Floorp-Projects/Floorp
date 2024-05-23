/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test case verifies the counts and extra data sent from telemetry events when interacting
 * with the SelectTranslationsPanel's primary happy-path UI states.
 */
add_task(
  async function test_select_translations_panel_telemetry_primary_ui_components() {
    const { cleanup, runInPage, resolveDownloads } = await loadTestPage({
      page: SELECT_TEST_PAGE_URL,
      languagePairs: [
        { fromLang: "es", toLang: "en" },
        { fromLang: "en", toLang: "es" },
        { fromLang: "fa", toLang: "en" },
        { fromLang: "en", toLang: "fa" },
        { fromLang: "fi", toLang: "en" },
        { fromLang: "en", toLang: "fi" },
        { fromLang: "fr", toLang: "en" },
        { fromLang: "en", toLang: "fr" },
        { fromLang: "sl", toLang: "en" },
        { fromLang: "en", toLang: "sl" },
        { fromLang: "uk", toLang: "en" },
        { fromLang: "en", toLang: "uk" },
      ],
      prefs: [["browser.translations.select.enable", true]],
    });

    await SelectTranslationsTestUtils.openPanel(runInPage, {
      selectSpanishSentence: true,
      openAtSpanishSentence: true,
      expectedFromLanguage: "es",
      expectedToLanguage: "en",
      downloadHandler: resolveDownloads,
      onOpenPanel: SelectTranslationsTestUtils.assertPanelViewTranslated,
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
          value => value.extra.source_text_code_units === "165",
          value => value.extra.source_text_word_count === "28",
        ],
      }
    );

    await SelectTranslationsTestUtils.clickCopyButton();
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.copyButton,
      {
        expectedEventCount: 1,
      }
    );

    await SelectTranslationsTestUtils.changeSelectedFromLanguage(["fi"], {
      openDropdownMenu: false,
      downloadHandler: resolveDownloads,
      onChangeLanguage: SelectTranslationsTestUtils.assertPanelViewTranslated,
    });
    await TestTranslationsTelemetry.assertEvent(
      Glean.translations.translationRequest,
      {
        expectedEventCount: 2,
        finalValuePredicates: [
          value => value.extra.document_language === "es",
          value => value.extra.from_language === "fi",
          value => value.extra.to_language === "en",
          value => value.extra.top_preferred_language === "en",
          value => value.extra.request_target === "select",
          value => value.extra.auto_translate === "false",
          value => value.extra.source_text_code_units === "165",
          value => value.extra.source_text_word_count === "28",
        ],
      }
    );

    await SelectTranslationsTestUtils.clickCopyButton();
    await SelectTranslationsTestUtils.clickCopyButton();
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.copyButton,
      {
        expectedEventCount: 3,
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
        expectedEventCount: 1,
      }
    );

    await SelectTranslationsTestUtils.openPanel(runInPage, {
      selectSpanishSentence: true,
      openAtSpanishSentence: true,
      expectedFromLanguage: "es",
      expectedToLanguage: "en",
      onOpenPanel: SelectTranslationsTestUtils.assertPanelViewTranslated,
    });
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.open,
      {
        expectedEventCount: 2,
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
      Glean.translations.translationRequest,
      {
        expectedEventCount: 3,
        finalValuePredicates: [
          value => value.extra.document_language === "es",
          value => value.extra.from_language === "es",
          value => value.extra.to_language === "en",
          value => value.extra.top_preferred_language === "en",
          value => value.extra.request_target === "select",
          value => value.extra.auto_translate === "false",
          value => value.extra.source_text_code_units === "165",
          value => value.extra.source_text_word_count === "28",
        ],
      }
    );

    await SelectTranslationsTestUtils.changeSelectedToLanguage(["fa"], {
      openDropdownMenu: true,
      pivotTranslation: true,
      downloadHandler: resolveDownloads,
      onChangeLanguage: SelectTranslationsTestUtils.assertPanelViewTranslated,
    });
    await TestTranslationsTelemetry.assertEvent(
      Glean.translations.translationRequest,
      {
        expectedEventCount: 4,
        finalValuePredicates: [
          value => value.extra.document_language === "es",
          value => value.extra.from_language === "es",
          value => value.extra.to_language === "fa",
          value => value.extra.top_preferred_language === "en",
          value => value.extra.request_target === "select",
          value => value.extra.auto_translate === "false",
          value => value.extra.source_text_code_units === "165",
          value => value.extra.source_text_word_count === "28",
        ],
      }
    );

    await SelectTranslationsTestUtils.clickCopyButton();
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.copyButton,
      {
        expectedEventCount: 4,
      }
    );

    await SelectTranslationsTestUtils.clickTranslateFullPageButton();
    await TestTranslationsTelemetry.assertEvent(
      Glean.translationsSelectTranslationsPanel.translateFullPageButton,
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
    await TestTranslationsTelemetry.assertEvent(
      Glean.translations.translationRequest,
      {
        expectedEventCount: 5,
        finalValuePredicates: [
          value => value.extra.document_language === "es",
          value => value.extra.from_language === "es",
          value => value.extra.to_language === "fa",
          value => value.extra.top_preferred_language === "en",
          value => value.extra.request_target === "full-page",
          value => value.extra.auto_translate === "false",
        ],
      }
    );

    await FullPageTranslationsTestUtils.assertPageIsTranslated(
      "es",
      "fa",
      runInPage
    );

    await cleanup();
  }
);
