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

  await assertPageIsUntranslated(runInPage);

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

  await assertPageIsTranslated(
    "es",
    "en",
    runInPage,
    "The page should be automatically translated."
  );

  info("Navigate to a different Spanish page");
  await navigate(SPANISH_PAGE_URL_DOT_ORG);

  await assertTranslationsButton(
    { button: true, circleArrows: true, locale: false, icon: true },
    "The icon presents the loading indicator."
  );

  await resolveDownloads(1);

  await assertPageIsTranslated(
    "es",
    "en",
    runInPage,
    "The page should be automatically translated."
  );

  info(
    "Simulate clicking always-translate-language in the settings menu " +
      "removing the document language from the alwaysTranslateLanguages pref"
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

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
    );

    await assertPageIsUntranslated(runInPage);

    await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });

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

    await assertPageIsTranslated("es", "en", runInPage);

    info(
      "Simulate clicking always-translate-language in the settings menu, " +
        "adding the document language to the alwaysTranslateLanguages pref"
    );
    await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await clickAlwaysTranslateLanguage();
    await assertIsAlwaysTranslateLanguage("es", { checked: true });

    await assertPageIsTranslated("es", "en", runInPage);

    info(
      "Simulate clicking always-translate-language in the settings menu " +
        "removing the document language from the alwaysTranslateLanguages pref"
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

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The translations button is visible."
    );

    await assertPageIsUntranslated(runInPage);

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

    await assertPageIsTranslated(
      "es",
      "en",
      runInPage,
      "The page should be automatically translated."
    );

    await navigate(
      SPANISH_PAGE_URL_DOT_ORG,
      "Navigate to a different Spanish page"
    );

    await assertTranslationsButton(
      { button: true, circleArrows: true, locale: false, icon: true },
      "The icon presents the loading indicator."
    );

    await resolveDownloads(1);

    await assertPageIsTranslated(
      "es",
      "en",
      runInPage,
      "The page should be automatically translated."
    );

    await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });

    await clickRestoreButton();

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is reverted to have an icon."
    );

    await assertPageIsUntranslated(runInPage);

    info(
      "Simulate clicking always-translate-language in the settings menu, " +
        "removing the document language to the alwaysTranslateLanguages pref"
    );
    await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await clickAlwaysTranslateLanguage();
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

    await assertPageIsUntranslated(runInPage);

    await cleanup();
  }
);
