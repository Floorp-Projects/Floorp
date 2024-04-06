/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test case tests the case of opening the SelectTranslationsPanel when the
 * detected language is unsupported, but the page language is known to be a supported
 * language. The panel should automatically fall back to the page language in an
 * effort to combat falsely identified selections.
 */
add_task(
  async function test_select_translations_panel_translate_sentence_on_open() {
    const { cleanup, runInPage, resolveDownloads } = await loadTestPage({
      page: SELECT_TEST_PAGE_URL,
      languagePairs: [
        // Do not include French.
        { fromLang: "es", toLang: "en" },
        { fromLang: "en", toLang: "es" },
      ],
      prefs: [["browser.translations.select.enable", true]],
    });

    await SelectTranslationsTestUtils.openPanel(runInPage, {
      selectFrenchSentence: true,
      openAtFrenchSentence: true,
      // French is not supported, but the page is in Spanish, so expect Spanish.
      expectedFromLanguage: "es",
      expectedToLanguage: "en",
      downloadHandler: resolveDownloads,
      onOpenPanel: SelectTranslationsTestUtils.assertPanelViewTranslated,
    });

    await SelectTranslationsTestUtils.clickDoneButton();

    await cleanup();
  }
);
