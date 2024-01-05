/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test case verifies the functionality of the translate-selection context menu item
 * when the selected text is not in the user's preferred language. The menu item should be
 * localized to translate to the target language matching the user's top preferred language
 * when the selected text is detected to be in a different language.
 */
add_task(
  async function test_translate_selection_menuitem_when_selected_text_is_not_preferred_language() {
    const { cleanup, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.select.enable", true]],
    });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
    );

    await assertPageIsUntranslated(runInPage);

    await assertContextMenuTranslateSelectionItem(
      runInPage,
      {
        selectSpanishParagraph: true,
        openAtSpanishParagraph: true,
        expectMenuItemVisible: true,
        expectedTargetLanguage: "en",
      },
      "The translate-selection context menu item should display a target language " +
        "when the selected text is not the preferred language."
    );

    await cleanup();
  }
);

/**
 * This test case verifies the functionality of the translate-selection context menu item
 * when the selected text is detected to be in the user's preferred language. The menu item
 * should not be localized to display a target language when the selected text matches the
 * user's top preferred language.
 */
add_task(
  async function test_translate_selection_menuitem_when_selected_text_is_preferred_language() {
    const { cleanup, runInPage } = await loadTestPage({
      page: ENGLISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.select.enable", true]],
    });

    await assertTranslationsButton(
      { button: false },
      "The button is available."
    );

    await assertContextMenuTranslateSelectionItem(
      runInPage,
      {
        selectFirstParagraph: true,
        openAtFirstParagraph: true,
        expectMenuItemVisible: true,
        expectedTargetLanguage: null,
      },
      "The translate-selection context menu item should not display a target language " +
        "when the selected text is in the preferred language."
    );

    await cleanup();
  }
);
