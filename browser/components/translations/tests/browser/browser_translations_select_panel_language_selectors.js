/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  async function test_select_translations_panel_open_spanish_language_selectors() {
    const { cleanup, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.select.enable", true]],
    });

    await SelectTranslationsTestUtils.openPanel(runInPage, {
      selectSpanishParagraph: true,
      openAtSpanishParagraph: true,
      expectedTargetLanguage: "en",
      onOpenPanel: SelectTranslationsTestUtils.assertPanelViewDefault,
    });

    SelectTranslationsTestUtils.assertSelectedFromLanguage({ langTag: "es" });
    SelectTranslationsTestUtils.assertSelectedToLanguage({ langTag: "en" });

    await SelectTranslationsTestUtils.clickDoneButton();

    await cleanup();
  }
);

add_task(
  async function test_select_translations_panel_open_english_language_selectors() {
    const { cleanup, runInPage } = await loadTestPage({
      page: ENGLISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.select.enable", true]],
    });

    await SelectTranslationsTestUtils.openPanel(runInPage, {
      selectFirstParagraph: true,
      openAtFirstParagraph: true,
      expectedTargetLanguage: "en",
      onOpenPanel: SelectTranslationsTestUtils.assertPanelViewDefault,
    });

    SelectTranslationsTestUtils.assertSelectedFromLanguage({ langTag: "en" });
    SelectTranslationsTestUtils.assertSelectedToLanguage({
      l10nId: "translations-panel-choose-language",
    });

    await SelectTranslationsTestUtils.clickDoneButton();

    await cleanup();
  }
);
