/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the effect of unchecking the always-translate language menuitem after the page has
 * been manually restored to its original form.
 * This should have no effect on the page, and further page loads should no longer auto-translate.
 */
add_task(
  async function test_deactivate_always_translate_language_after_restore() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
    });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The translations button is visible."
    );

    await assertPageIsUntranslated(runInPage);

    await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await clickAlwaysTranslateLanguage({
      downloadHandler: resolveDownloads,
    });
    await assertIsAlwaysTranslateLanguage("es", { checked: true });

    await assertPageIsTranslated(
      "es",
      "en",
      runInPage,
      "The page should be automatically translated."
    );

    await navigate("Navigate to a different Spanish page", {
      url: SPANISH_PAGE_URL_DOT_ORG,
      downloadHandler: resolveDownloads,
    });

    await assertPageIsTranslated(
      "es",
      "en",
      runInPage,
      "The page should be automatically translated."
    );

    await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });

    await clickRestoreButton();

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is reverted to have an icon."
    );

    await assertPageIsUntranslated(runInPage);

    await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await clickAlwaysTranslateLanguage();
    await assertIsAlwaysTranslateLanguage("es", { checked: false });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button shows only the icon."
    );

    await navigate("Reload the page", { url: SPANISH_PAGE_URL_DOT_ORG });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button shows only the icon."
    );

    await assertPageIsUntranslated(runInPage);

    await cleanup();
  }
);
