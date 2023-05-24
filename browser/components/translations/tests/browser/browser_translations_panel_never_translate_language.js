/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the effect that toggling the never-translate-languages menuitem
 * has on subsequent page loads.
 */
add_task(async function test_page_loads_with_never_translate_language() {
  const { cleanup, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.neverTranslateLanguages", "pl,fr"]],
  });

  await assertTranslationsButton(
    { button: true },
    "The translations button should be visible"
  );

  info(
    'The document language "es" is not in the neverTranslateLanguages pref, ' +
      "so the page should be untranslated, in its original form."
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
    "Simulate clicking never-translate-language in the settings menu, " +
      "adding the document language from the neverTranslateLanguages pref"
  );
  await openSettingsMenu();
  await toggleNeverTranslateLanguage();

  // Reload the page
  await navigate(SPANISH_PAGE_URL);

  await assertTranslationsButton(
    { button: false },
    "The translations button should be invisible"
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

  await cleanup();
});
