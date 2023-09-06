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
    permissionsUrls: [SPANISH_PAGE_URL],
  });

  await assertPageIsUntranslated(runInPage);

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The translations button is visible."
  );

  info(
    "Simulate clicking never-translate-site in the settings menu, " +
      "denying translations permissions for this content window principal"
  );
  await openTranslationsSettingsMenuViaTranslationsButton();

  await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });
  await clickNeverTranslateSite();
  await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

  info(
    "Simulate clicking always-translate-language in the settings menu, " +
      "adding the document language to the alwaysTranslateLanguages pref"
  );
  await openTranslationsSettingsMenuViaAppMenu();

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
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      permissionsUrls: [SPANISH_PAGE_URL],
      prefs: [["browser.translations.alwaysTranslateLanguages", "es"]],
    });

    await assertTranslationsButton(
      { button: true, circleArrows: true, locale: false, icon: true },
      "The icon presents the loading indicator."
    );

    await resolveDownloads(1);

    await assertPageIsTranslated("es", "en", runInPage);

    info(
      "Simulate clicking never-translate-site in the settings menu, " +
        "denying translations permissions for this content window principal"
    );
    await openTranslationsSettingsMenuViaTranslationsButton();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

    await clickNeverTranslateSite();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

    await assertPageIsUntranslated(runInPage);

    info(
      "Simulate clicking never-translate-site in the settings menu, " +
        "regranting translations permissions for this content window principal"
    );
    await openTranslationsSettingsMenuViaAppMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

    await clickNeverTranslateSite();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

    await assertPageIsTranslated("es", "en", runInPage);

    await cleanup();
  }
);
