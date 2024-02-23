/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test case verifies the visibility and initial state of UI elements within the
 * Select Translations Panel's main-view UI.
 */
add_task(
  async function test_select_translations_panel_mainview_ui_element_visibility() {
    const { cleanup, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.select.enable", true]],
    });

    await FullPageTranslationsTestUtils.assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
    );

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await SelectTranslationsTestUtils.openPanel(runInPage, {
      selectSpanishParagraph: true,
      openAtSpanishParagraph: true,
      expectedTargetLanguage: "es",
      onOpenPanel: SelectTranslationsTestUtils.assertPanelViewDefault,
    });

    await SelectTranslationsTestUtils.clickDoneButton();

    await cleanup();
  }
);
