/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the effects that toggling the always-translate-languages menuitem
 * has on subsequent page loads.
 */
add_task(async function test_page_loads_with_always_translate_language() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.alwaysTranslateLanguages", "pl,fr"]],
  });

  await assertTranslationsButton(
    { button: true },
    "The translations button should be visible"
  );

  info(
    'The document language "es" is not in the alwaysTranslateLanguages pref, ' +
      "so the page should be untranslated, in its original form"
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
      "adding the document language from the alwaysTranslateLanguages pref"
  );
  await openSettingsMenu();
  await toggleAlwaysTranslateLanguage();

  await navigate(SPANISH_PAGE_URL, "Reload the page");

  resolveDownloads(1);

  info(
    "The page should now be automatically translated because the document language " +
      "should be added to the always-translate pref"
  );
  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is translated automatically",
      getH1,
      "DON QUIJOTE DE LA MANCHA [es to en, html]"
    );
  });

  info(
    "Simulate clicking always-translate-language in the settings menu " +
      "removing the document language from the alwaysTranslateLanguages pref"
  );
  await openSettingsMenu();
  await toggleAlwaysTranslateLanguage();

  await navigate(SPANISH_PAGE_URL, "Reload the page");

  info(
    "The page should no longer automatically translated because the document language " +
      "should be removed from the always-translate pref"
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
