/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the effect of toggling the never-translate-site menuitem.
 * Checking the box on an untranslated page should immediately hide the button.
 * The button should not appear again for sites that share the same content principal
 * of the disabled site.
 */
add_task(async function test_toggle_never_translate_site_menuitem() {
  const { cleanup, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    permissionsUrls: [SPANISH_PAGE_URL],
  });

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The translations button is visible."
  );

  info(
    "Translations permissions are currently allowed for this test page " +
      "and the page should be untranslated, in its original form."
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
    "Simulate clicking never-translate-site in the settings menu, " +
      "denying translations permissions for this content window principal"
  );
  await openTranslationsSettingsMenuViaTranslationsButton();

  await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });
  await toggleNeverTranslateSite();
  await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

  info("The page should still be in its original, untranslated form");
  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  await navigate(SPANISH_PAGE_URL, "Reload the page");

  info("The page should still be in its original, untranslated form");
  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  await navigate(
    SPANISH_PAGE_URL_2,
    "Navigate to a Spanish page with the same content principal"
  );

  info("The page should still be in its original, untranslated form");
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
    "Navigate to a Spanish page with a different content principal"
  );

  await assertTranslationsButton(
    { button: true },
    "The translations button should be visible, because this content principal " +
      "has not been denied translations permissions"
  );

  info("The page should still be in its original, untranslated form");
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
 * Tests the effect of toggling the never-translate-site menuitem on a page where
 * where translation is already active.
 * Checking the box on a translated page should restore the page and hide the button.
 * The button should not appear again for sites that share the same content principal
 * of the disabled site.
 */
add_task(
  async function test_toggle_never_translate_site_menuitem_with_active_translations() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      permissionsUrls: [SPANISH_PAGE_URL],
    });

    const { button } = await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The translations button is visible."
    );

    info(
      "Translations permissions are currently allowed for this test page " +
        "and the page should be untranslated, in its original form."
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
      "Simulate clicking never-translate-site in the settings menu, " +
        "denying translations permissions for this content window principal"
    );
    await openTranslationsSettingsMenuViaTranslationsButton();

    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });
    await toggleNeverTranslateSite();
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

    info("The page should still be in its original, untranslated form");
    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is in Spanish.",
        getH1,
        "Don Quijote de La Mancha"
      );
    });

    await navigate(SPANISH_PAGE_URL, "Reload the page");

    info("The page should still be in its original, untranslated form");
    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is in Spanish.",
        getH1,
        "Don Quijote de La Mancha"
      );
    });

    await navigate(
      SPANISH_PAGE_URL_2,
      "Navigate to a Spanish page with the same content principal"
    );

    info("The page should still be in its original, untranslated form");
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
      "Navigate to a Spanish page with a different content principal"
    );

    await assertTranslationsButton(
      { button: true },
      "The translations button should be visible, because this content principal " +
        "has not been denied translations permissions"
    );

    info("The page should still be in its original, untranslated form");
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
 * Tests the effect of toggling the never-translate-site menuitem on a page where
 * where translation is already active via always-translate.
 * Checking the box on a translated page should restore the page and hide the button.
 * The button should not appear again for sites that share the same content principal
 * of the disabled site, and no auto-translation should occur.
 * Other sites should still auto-translate for this language.
 */
add_task(
  async function test_toggle_never_translate_site_menuitem_with_always_translate_active() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.alwaysTranslateLanguages", "uk,it"]],
      permissionsUrls: [SPANISH_PAGE_URL],
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
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

    await toggleAlwaysTranslateLanguage();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

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
      "Simulate clicking never-translate-site in the settings menu, " +
        "denying translations permissions for this content window principal"
    );
    await openTranslationsSettingsMenuViaTranslationsButton();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

    await toggleNeverTranslateSite();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

    info("The page should still be in its original, untranslated form");
    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is in Spanish.",
        getH1,
        "Don Quijote de La Mancha"
      );
    });

    await navigate(SPANISH_PAGE_URL, "Reload the page");

    info("The page should still be in its original, untranslated form");
    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is in Spanish.",
        getH1,
        "Don Quijote de La Mancha"
      );
    });

    await navigate(
      SPANISH_PAGE_URL_2,
      "Navigate to a Spanish page with the same content principal"
    );

    info("The page should still be in its original, untranslated form");
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
      "Navigate to a Spanish page with a different content principal"
    );

    await assertTranslationsButton(
      { button: true, circleArrows: true, locale: false, icon: true },
      "The icon presents the loading indicator."
    );

    await resolveDownloads(1);

    await assertTranslationsButton(
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

    await cleanup();
  }
);
