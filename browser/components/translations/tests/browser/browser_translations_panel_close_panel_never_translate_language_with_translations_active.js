"use strict";

/**
 * Tests the effect of checking the never-translate-language menuitem on a page where
 * translations are active (always-translate-language is enabled).
 * Checking the box on the page automatically closes/hides the translations panel.
 */
add_task(
  async function test_panel_closes_on_toggle_never_translate_language_with_translations_active() {
    const { cleanup, runInPage, resolveDownloads } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
    });

    await assertTranslationsButton(
      { button: true },
      "The translations button is available"
    );

    await openTranslationsPanel({
      openFromAppMenu: true,
      onOpenPanel: assertPanelDefaultView,
    });

    await openTranslationsSettingsMenu();
    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await clickAlwaysTranslateLanguage({
      downloadHandler: resolveDownloads,
    });
    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertPageIsTranslated(
      "es",
      "en",
      runInPage,
      "The page should be automatically translated."
    );

    await openTranslationsPanel({
      onOpenPanel: assertPanelRevisitView,
    });
    await openTranslationsSettingsMenu();
    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateLanguage("es", { checked: false });
    await waitForTranslationsPopupEvent("popuphidden", async () => {
      await clickNeverTranslateLanguage();
    });

    await assertPageIsUntranslated(runInPage);
    await cleanup();
  }
);

/**
 * Tests the effect of checking the never-translate-language menuitem on
 * a page where translations are active through always-translate-language
 * and inactive on a site through never-translate-site.
 * Checking the box on the page automatically closes/hides the translations panel.
 */
add_task(
  async function test_panel_closes_on_toggle_never_translate_language_with_always_translate_language_and_never_translate_site_active() {
    const { cleanup, runInPage, resolveDownloads } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
    });

    await assertTranslationsButton(
      { button: true },
      "The translations button is available"
    );

    await openTranslationsPanel({
      openFromAppMenu: true,
      onOpenPanel: assertPanelDefaultView,
    });

    await openTranslationsSettingsMenu();
    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await clickAlwaysTranslateLanguage({
      downloadHandler: resolveDownloads,
    });
    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertPageIsTranslated(
      "es",
      "en",
      runInPage,
      "The page should be automatically translated."
    );

    await openTranslationsPanel({
      onOpenPanel: assertPanelRevisitView,
    });
    await openTranslationsSettingsMenu();
    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });
    await clickNeverTranslateSite();
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });
    await assertPageIsUntranslated(runInPage);

    await openTranslationsPanel({
      openFromAppMenu: true,
      onOpenPanel: assertPanelDefaultView,
    });
    await openTranslationsSettingsMenu();
    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });
    await assertPageIsUntranslated(runInPage);

    await assertIsNeverTranslateLanguage("es", { checked: false });
    await waitForTranslationsPopupEvent("popuphidden", async () => {
      await clickNeverTranslateLanguage();
    });
    await cleanup();
  }
);
