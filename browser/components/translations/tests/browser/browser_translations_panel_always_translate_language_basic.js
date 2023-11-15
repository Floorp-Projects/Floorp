/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the effect of toggling the always-translate-language menuitem.
 * Checking the box on an untranslated page should immediately translate the page.
 * Unchecking the box on a translated page should immediately restore the page.
 */
add_task(async function test_toggle_always_translate_language_menuitem() {
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
  await openTranslationsSettingsMenu();

  await assertIsAlwaysTranslateLanguage("es", { checked: true });
  await clickAlwaysTranslateLanguage();
  await assertIsAlwaysTranslateLanguage("es", { checked: false });

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "Only the button appears"
  );

  await assertPageIsUntranslated(runInPage);

  await cleanup();
});
