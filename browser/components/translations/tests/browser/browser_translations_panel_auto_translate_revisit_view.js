/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This tests a specific defect where the revisit view was not showing properly
 * when navigating to an auto-translated page after visiting a page in an unsupported
 * language and viewing the panel.
 *
 * This test case tests the case where the auto translate succeeds and the user
 * manually opens the panel to show the revisit view.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1845611 for more information.
 */
add_task(
  async function test_revisit_view_updates_with_auto_translate_success() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: [
        // Do not include French.
        { fromLang: "en", toLang: "es" },
        { fromLang: "es", toLang: "en" },
      ],
    });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The translations button is visible."
    );

    await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await clickAlwaysTranslateLanguage({
      downloadHandler: resolveDownloads,
    });
    await assertIsAlwaysTranslateLanguage("es", { checked: true });

    await assertPageIsTranslated("es", "en", runInPage);

    await navigate("Navigate to a page in an unsupported language", {
      url: FRENCH_PAGE_URL,
    });

    await assertTranslationsButton(
      { button: false },
      "The translations button should be unavailable."
    );

    await openTranslationsPanel({
      openFromAppMenu: true,
      onOpenPanel: assertPanelUnsupportedLanguageView,
    });

    await navigate("Navigate back to the Spanish page.", {
      url: SPANISH_PAGE_URL_DOT_ORG,
      downloadHandler: resolveDownloads,
    });

    await assertPageIsTranslated("es", "en", runInPage);

    await openTranslationsPanel({
      openFromAppMenu: true,
      onOpenPanel: assertPanelRevisitView,
    });

    await cleanup();
  }
);
