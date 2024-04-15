/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test case verifies the select translations panel's functionality when opened with an unsupported
 * from-language, ensuring it opens with the correct view with no from-language selected.
 */
add_task(
  async function test_select_translations_panel_open_no_selected_from_lang() {
    const { cleanup, runInPage } = await loadTestPage({
      page: SELECT_TEST_PAGE_URL,
      languagePairs: [
        // Do not include Spanish.
        { fromLang: "fr", toLang: "en" },
        { fromLang: "en", toLang: "fr" },
      ],
      prefs: [["browser.translations.select.enable", true]],
    });

    await SelectTranslationsTestUtils.openPanel(runInPage, {
      selectSpanishSentence: true,
      openAtSpanishSentence: true,
      expectedFromLanguage: null,
      expectedToLanguage: "en",
      onOpenPanel:
        SelectTranslationsTestUtils.assertPanelViewNoFromLangSelected,
    });

    await SelectTranslationsTestUtils.clickDoneButton();

    await cleanup();
  }
);
