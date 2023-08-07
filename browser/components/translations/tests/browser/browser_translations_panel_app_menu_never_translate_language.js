/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the effect of unchecking the never-translate-language menuitem, removing
 * the language from the never-translate languages list.
 * The translations button should reappear.
 */
add_task(async function test_uncheck_never_translate_language_shows_button() {
  const { cleanup, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.neverTranslateLanguages", "es"]],
  });

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The translations button is available"
  );

  info(
    "The page should still be in its original, untranslated form because " +
      "the document language is in the neverTranslateLanguages pref"
  );
  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  info(
    "Simulate clicking always-translate-language in the settings menu, " +
      "adding the document language to the alwaysTranslateLanguages pref"
  );
  await openTranslationsSettingsMenuViaAppMenu();

  await assertIsNeverTranslateLanguage("es", { checked: true });
  await toggleNeverTranslateLanguage();
  await assertIsNeverTranslateLanguage("es", { checked: false });

  await cleanup();
});
