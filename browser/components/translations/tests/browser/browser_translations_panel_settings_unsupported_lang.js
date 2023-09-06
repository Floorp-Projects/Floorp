/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This tests a specific defect where the language checkbox states were not being
 * updated correctly when visiting a web page in an unsupported language after
 * previously enabling always-translate-language or never-translate-language
 * on a site with a supported language.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1845611 for more information.
 */
add_task(async function test_unsupported_language_settings_menu_checkboxes() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: [
      // Do not include French.
      { fromLang: "en", toLang: "es" },
      { fromLang: "es", toLang: "en" },
    ],
    prefs: [["browser.translations.alwaysTranslateLanguages", ""]],
  });

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The translations button is visible."
  );

  info(
    "Simulate clicking always-translate-language in the settings menu, " +
      "adding the document language to the alwaysTranslateLanguages pref"
  );
  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });
  await openTranslationsSettingsMenu();

  await assertIsAlwaysTranslateLanguage("es", { checked: false });
  await clickAlwaysTranslateLanguage();
  await assertIsAlwaysTranslateLanguage("es", { checked: true });

  await assertTranslationsButton(
    { button: true, circleArrows: true, locale: false, icon: true },
    "The icon presents the loading indicator."
  );

  await resolveDownloads(1);

  await assertPageIsTranslated("es", "en", runInPage);

  await navigate(
    FRENCH_PAGE_URL,
    "Navigate to a page in an unsupported language."
  );

  await assertTranslationsButton(
    { button: false },
    "The translations button should be unavailable."
  );

  await openTranslationsPanel({
    openFromAppMenu: true,
    onOpenPanel: assertPanelUnsupportedLanguageView,
  });
  assertIsAlwaysTranslateLanguage("fr", { checked: false, disabled: true });
  assertIsNeverTranslateLanguage("fr", { checked: false, disabled: true });

  await cleanup();
});
