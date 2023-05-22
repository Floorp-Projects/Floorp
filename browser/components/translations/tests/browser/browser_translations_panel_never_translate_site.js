/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the effects that the never-translate-site menuitem has on
 * subsequent page loads.
 */
add_task(async function test_page_loads_with_never_translate_site() {
  const { cleanup, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    permissionsUrls: [SPANISH_PAGE_URL],
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

  info("Disallow translations for this site");
  await openSettingsMenu();
  await toggleNeverTranslateSite();

  info("Reload the page");
  await navigate(SPANISH_PAGE_URL);

  await assertTranslationsButton(
    { button: false },
    "The translations button should be invisible"
  );

  info(
    "The page should no longer automatically translated because the site " +
      "no longer has permissions to be translated"
  );
  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  info("Go to another page from the same site principal");
  await navigate(SPANISH_PAGE_URL_2);

  await assertTranslationsButton(
    { button: false },
    "The translations button should be invisible"
  );

  info(
    "This page should also be untranslated because the entire site " +
      "no longer has permissions to be translated"
  );
  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  info("Go to another page from another site principal");
  await navigate(SPANISH_PAGE_URL_DOT_ORG);

  await assertTranslationsButton(
    { button: true },
    "The translations button should be visible"
  );

  info(
    "This page should be untranslated because there are no auto-translate " +
      "preferences set in this test"
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

/**
 * Tests the effects that the never-translate-site menuitem has on
 * subsequent page loads when always-translate-language is active.
 */
add_task(
  async function test_page_loads_with_always_translate_language_and_never_translate_site() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.alwaysTranslateLanguages", "es"]],
      permissionsUrls: [SPANISH_PAGE_URL],
    });

    await assertTranslationsButton(
      { button: true },
      "The translations button should be visible"
    );

    resolveDownloads(1);

    info(
      "The page should be automatically translated because the document language " +
        "should be in the alwaysTranslateLanguages"
    );
    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is translated automatically",
        getH1,
        "DON QUIJOTE DE LA MANCHA [es to en, html]"
      );
    });

    info("Disallow translations for this site");
    await openSettingsMenu();
    await toggleNeverTranslateSite();

    info("Reload the page");
    await navigate(SPANISH_PAGE_URL);

    await assertTranslationsButton(
      { button: false },
      "The translations button should be invisible"
    );

    info(
      "The page should no longer automatically translated because the site " +
        "no longer has permissions to be translated"
    );
    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is in Spanish.",
        getH1,
        "Don Quijote de La Mancha"
      );
    });

    info("Go to another page from the same site principal");
    await navigate(SPANISH_PAGE_URL_2);

    await assertTranslationsButton(
      { button: false },
      "The translations button should be invisible"
    );

    info(
      "This page should also be untranslated because the entire site " +
        "no longer has permissions to be translated"
    );
    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is in Spanish.",
        getH1,
        "Don Quijote de La Mancha"
      );
    });

    info("Go to another page from a different site principal");
    await navigate(SPANISH_PAGE_URL_DOT_ORG);

    await assertTranslationsButton(
      { button: true },
      "The translations button should be visible"
    );

    resolveDownloads(1);

    info(
      "The page should be automatically translated because the document language " +
        "should be in the alwaysTranslateLanguages and this is a different site principal"
    );
    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is translated automatically",
        getH1,
        "DON QUIJOTE DE LA MANCHA [es to en, html]"
      );
    });

    await cleanup();
  }
);
