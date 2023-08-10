/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the effect of toggling the never-translate-language menuitem.
 * Checking the box on an untranslated page should immediately hide the button.
 * The button should not appear again for sites in the disabled language.
 */
add_task(async function test_toggle_never_translate_language_menuitem() {
  const { cleanup, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.neverTranslateLanguages", "pl,fr"]],
  });

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The translations button is visible."
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
  await openTranslationsSettingsMenuViaTranslationsButton();

  await assertIsNeverTranslateLanguage("es", { checked: false });
  await toggleNeverTranslateLanguage();
  await assertIsNeverTranslateLanguage("es", { checked: true });

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

  await navigate(SPANISH_PAGE_URL, "Reload the page");

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

  await navigate(
    SPANISH_PAGE_URL_DOT_ORG,
    "Navigate to a different Spanish page"
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

/**
 * Tests the effect of toggling the never-translate-language menuitem on a page where
 * where translation is already active.
 * Checking the box on a translated page should restore the page and hide the button.
 * The button should not appear again for sites in the disabled language.
 */
add_task(
  async function test_toggle_never_translate_language_menuitem_with_active_translations() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.neverTranslateLanguages", "pl,fr"]],
    });

    const { button } = await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
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

    await waitForTranslationsPopupEvent(
      "popupshown",
      () => {
        click(button, "Opening the popup");
      },
      assertPanelDefaultView
    );

    await waitForTranslationsPopupEvent("popuphidden", () => {
      click(
        getByL10nId("translations-panel-translate-button"),
        "Start translating by clicking the translate button."
      );
    });

    await assertTranslationsButton(
      { button: true, circleArrows: true, locale: false, icon: true },
      "The icon presents the loading indicator."
    );

    await resolveDownloads(1);

    const { locale } = await assertTranslationsButton(
      { button: true, circleArrows: false, locale: true, icon: true },
      "The icon presents the locale."
    );

    is(locale.innerText, "en", "The English language tag is shown.");

    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The pages H1 is translated.",
        getH1,
        "DON QUIJOTE DE LA MANCHA [es to en, html]"
      );
    });

    info(
      "Simulate clicking never-translate-language in the settings menu, " +
        "adding the document language from the neverTranslateLanguages pref"
    );
    await openTranslationsSettingsMenuViaTranslationsButton();

    await assertIsNeverTranslateLanguage("es", { checked: false });
    await toggleNeverTranslateLanguage();
    await assertIsNeverTranslateLanguage("es", { checked: true });

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

    await navigate(SPANISH_PAGE_URL, "Reload the page");

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
  }
);

/**
 * Tests the effect of toggling the never-translate-language menuitem on a page where
 * where translation is already active via always-translate.
 * Checking the box on a translated page should restore the page and hide the button.
 * The language should be moved from always-translate to never-translate.
 * The button should not appear again for sites in the disabled language.
 */
add_task(
  async function test_toggle_never_translate_language_menuitem_with_always_translate_active() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [
        ["browser.translations.alwaysTranslateLanguages", "uk,it"],
        ["browser.translations.neverTranslateLanguages", "pl,fr"],
      ],
    });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
    );

    info(
      "Simulate clicking always-translate-language in the settings menu, " +
        "adding the document language to the alwaysTranslateLanguages pref"
    );
    await openTranslationsSettingsMenuViaTranslationsButton();

    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await assertIsNeverTranslateLanguage("es", { checked: false });

    await toggleAlwaysTranslateLanguage();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateLanguage("es", { checked: false });

    await assertTranslationsButton(
      { button: true, circleArrows: true, locale: false, icon: true },
      "The icon presents the loading indicator."
    );

    await resolveDownloads(1);

    const { locale } = await assertTranslationsButton(
      { button: true, circleArrows: false, locale: true, icon: true },
      "The icon presents the locale."
    );

    is(locale.innerText, "en", "The English language tag is shown.");

    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The pages H1 is translated.",
        getH1,
        "DON QUIJOTE DE LA MANCHA [es to en, html]"
      );
    });

    info(
      "Simulate clicking never-translate-language in the settings menu, " +
        "adding the document language from the neverTranslateLanguages pref " +
        "and removing it from the alwaysTranslateLanguages pref"
    );
    await openTranslationsSettingsMenuViaTranslationsButton();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateLanguage("es", { checked: false });

    await toggleNeverTranslateLanguage();

    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await assertIsNeverTranslateLanguage("es", { checked: true });

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

    await navigate(SPANISH_PAGE_URL, "Reload the page");

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
  }
);
