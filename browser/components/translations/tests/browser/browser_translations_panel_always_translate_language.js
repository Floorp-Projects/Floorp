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
    prefs: [["browser.translations.alwaysTranslateLanguages", "pl,fr"]],
  });

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The translations button is visible."
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
      "adding the document language to the alwaysTranslateLanguages pref"
  );
  await openTranslationsSettingsMenuViaTranslationsButton();

  await assertIsAlwaysTranslateLanguage("es", { checked: false });
  await toggleAlwaysTranslateLanguage();
  await assertIsAlwaysTranslateLanguage("es", { checked: true });

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

  info("Navigate to a different Spanish page");
  await navigate(SPANISH_PAGE_URL_DOT_ORG);

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
  await openTranslationsSettingsMenuViaTranslationsButton();

  await assertIsAlwaysTranslateLanguage("es", { checked: true });
  await toggleAlwaysTranslateLanguage();
  await assertIsAlwaysTranslateLanguage("es", { checked: false });

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "Only the button appears"
  );

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

/**
 * Tests the effect of toggling the always-translate-language menuitem after the page has
 * been manually translated. This should not reload or retranslate the page, but just check
 * the box.
 */
add_task(
  async function test_activate_always_translate_language_after_manual_translation() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.alwaysTranslateLanguages", "pl,fr"]],
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
      "Simulate clicking always-translate-language in the settings menu, " +
        "adding the document language to the alwaysTranslateLanguages pref"
    );
    await openTranslationsSettingsMenuViaTranslationsButton();

    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await toggleAlwaysTranslateLanguage();
    await assertIsAlwaysTranslateLanguage("es", { checked: true });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: true, icon: true },
      "The continues to present the locale without pending downloads."
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
      "Simulate clicking always-translate-language in the settings menu " +
        "removing the document language from the alwaysTranslateLanguages pref"
    );
    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await toggleAlwaysTranslateLanguage();
    await assertIsAlwaysTranslateLanguage("es", { checked: false });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "Only the button appears"
    );

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
  }
);

/**
 * Tests the effect of unchecking the always-translate language menuitem after the page has
 * been manually restored to its original form.
 * This should have no effect on the page, and further page loads should no longer auto-translate.
 */
add_task(
  async function test_deactivate_always_translate_language_after_restore() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.alwaysTranslateLanguages", "pl,fr"]],
    });

    const { button } = await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The translations button is visible."
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
        "adding the document language to the alwaysTranslateLanguages pref"
    );
    await openTranslationsSettingsMenuViaTranslationsButton();

    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await toggleAlwaysTranslateLanguage();
    await assertIsAlwaysTranslateLanguage("es", { checked: true });

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

    await navigate(
      SPANISH_PAGE_URL_DOT_ORG,
      "Navigate to a different Spanish page"
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

    await waitForTranslationsPopupEvent(
      "popupshown",
      () => {
        click(button, "Re-opening the popup");
      },
      assertPanelRevisitView
    );

    await waitForTranslationsPopupEvent("popuphidden", () => {
      click(
        getByL10nId("translations-panel-restore-button"),
        "Click the restore language button."
      );
    });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is reverted to have an icon."
    );

    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is restored to Spanish.",
        getH1,
        "Don Quijote de La Mancha"
      );
    });

    info(
      "Simulate clicking always-translate-language in the settings menu, " +
        "removing the document language to the alwaysTranslateLanguages pref"
    );
    await openTranslationsSettingsMenuViaTranslationsButton();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await toggleAlwaysTranslateLanguage();
    await assertIsAlwaysTranslateLanguage("es", { checked: false });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button shows only the icon."
    );

    await navigate(SPANISH_PAGE_URL_DOT_ORG, "Reload the page");

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button shows only the icon."
    );

    await runInPage(async TranslationsTest => {
      const { getH1 } = TranslationsTest.getSelectors();
      await TranslationsTest.assertTranslationResult(
        "The page's H1 is restored to Spanish.",
        getH1,
        "Don Quijote de La Mancha"
      );
    });

    await cleanup();
  }
);
