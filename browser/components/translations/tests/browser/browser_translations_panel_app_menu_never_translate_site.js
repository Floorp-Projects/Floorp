/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the effect of unchecking the never-translate-site menuitem,
 * regranting translations permissions to this page.
 * The translations button should reappear.
 */
add_task(async function test_uncheck_never_translate_site_shows_button() {
  const { cleanup, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  await assertPageIsUntranslated(runInPage);

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The translations button is visible."
  );

  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });
  await openTranslationsSettingsMenu();

  await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });
  await clickNeverTranslateSite();
  await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

  await openTranslationsPanel({
    openFromAppMenu: true,
    onOpenPanel: assertPanelDefaultView,
  });
  await openTranslationsSettingsMenu();

  await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });
  await clickNeverTranslateSite();
  await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

  await cleanup();
});

/**
 * Tests the effect of unchecking the never-translate-site menuitem while
 * the current language is in the always-translate languages list, regranting
 * translations permissions to this page.
 * The page should automatically translate.
 */
add_task(
  async function test_uncheck_never_translate_site_with_always_translate_language() {
    const { cleanup, runInPage, resolveDownloads } = await loadTestPage({
      page: BLANK_PAGE,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.alwaysTranslateLanguages", "es"]],
    });

    await navigate("Navigate to a Spanish page", {
      url: SPANISH_PAGE_URL,
      downloadHandler: resolveDownloads,
    });

    await assertPageIsTranslated("es", "en", runInPage);

    await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

    await clickNeverTranslateSite();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

    await assertPageIsUntranslated(runInPage);

    await openTranslationsPanel({
      openFromAppMenu: true,
      onOpenPanel: assertPanelDefaultView,
    });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

    await clickNeverTranslateSite();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

    await assertPageIsTranslated("es", "en", runInPage);

    await cleanup();
  }
);
