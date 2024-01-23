/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test case checks the behavior of the translate-selection menu item in the context menu
 * when full-page translations is active or inactive. The menu item should be available under
 * the correct selected-text conditions while full-page translations is inactive, and it should
 * never be available while full-page translations is active.
 */
add_task(
  async function test_translate_selection_menuitem_with_text_selected_and_full_page_translations_active() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.select.enable", true]],
    });

    await FullPageTranslationsTestUtils.assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
    );

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await SelectTranslationsTestUtils.assertContextMenuTranslateSelectionItem(
      runInPage,
      {
        selectSpanishParagraph: true,
        openAtSpanishParagraph: true,
        expectMenuItemVisible: true,
        expectedTargetLanguage: "en",
      },
      "The translate-selection context menu item should be available while full-page translations is inactive."
    );

    await FullPageTranslationsTestUtils.openTranslationsPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
    });

    await FullPageTranslationsTestUtils.clickTranslateButton({
      downloadHandler: resolveDownloads,
    });

    await FullPageTranslationsTestUtils.assertPageIsTranslated(
      "es",
      "en",
      runInPage
    );

    await SelectTranslationsTestUtils.assertContextMenuTranslateSelectionItem(
      runInPage,
      {
        selectSpanishParagraph: true,
        openAtSpanishParagraph: true,
        expectMenuItemVisible: false,
      },
      "The translate-selection context menu item should be unavailable while full-page translations is active."
    );

    await FullPageTranslationsTestUtils.openTranslationsPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewRevisit,
    });

    await FullPageTranslationsTestUtils.clickRestoreButton();

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await SelectTranslationsTestUtils.assertContextMenuTranslateSelectionItem(
      runInPage,
      {
        selectSpanishParagraph: true,
        openAtSpanishParagraph: true,
        expectMenuItemVisible: true,
        expectedTargetLanguage: "en",
      },
      "The translate-selection context menu item should be available while full-page translations is inactive."
    );

    await cleanup();
  }
);

/**
 * This test case checks the behavior of the translate-selection menu item in the context menu
 * when full-page translations is active or inactive. The menu item should be available under
 * the correct link-clicked conditions while full-page translations is inactive, and it should
 * never be available while full-page translations is active.
 */
add_task(
  async function test_translate_selection_menuitem_with_link_clicked_and_full_page_translations_active() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.select.enable", true]],
    });

    await FullPageTranslationsTestUtils.assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
    );

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await SelectTranslationsTestUtils.assertContextMenuTranslateSelectionItem(
      runInPage,
      {
        selectSpanishParagraph: false,
        openAtSpanishHyperlink: true,
        expectMenuItemVisible: true,
        expectedTargetLanguage: "en",
      },
      "The translate-selection context menu item should be available while full-page translations is inactive."
    );

    await FullPageTranslationsTestUtils.openTranslationsPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
    });

    await FullPageTranslationsTestUtils.clickTranslateButton({
      downloadHandler: resolveDownloads,
    });

    await FullPageTranslationsTestUtils.assertPageIsTranslated(
      "es",
      "en",
      runInPage
    );

    await SelectTranslationsTestUtils.assertContextMenuTranslateSelectionItem(
      runInPage,
      {
        selectSpanishParagraph: false,
        openAtSpanishHyperlink: true,
        expectMenuItemVisible: false,
      },
      "The translate-selection context menu item should be unavailable while full-page translations is active."
    );

    await FullPageTranslationsTestUtils.openTranslationsPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewRevisit,
    });

    await FullPageTranslationsTestUtils.clickRestoreButton();

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await SelectTranslationsTestUtils.assertContextMenuTranslateSelectionItem(
      runInPage,
      {
        selectSpanishParagraph: false,
        openAtSpanishHyperlink: true,
        expectMenuItemVisible: true,
        expectedTargetLanguage: "en",
      },
      "The translate-selection context menu item should be available while full-page translations is inactive."
    );

    await cleanup();
  }
);
