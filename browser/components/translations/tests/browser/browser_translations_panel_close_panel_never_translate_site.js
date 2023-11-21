"use strict";

/**
 * Tests the effect of checking the never-translate-site menuitem on a page where
 * always-translate-language and never-translate-language are inactive.
 * Checking the box on the page automatically closes/hides the translations panel.
 */
add_task(async function test_panel_closes_on_toggle_never_translate_site() {
  const { cleanup } = await loadTestPage({
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

  await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });
  await waitForTranslationsPopupEvent("popuphidden", async () => {
    await clickNeverTranslateSite();
  });

  await cleanup();
});

/**
 * Tests the effect of checking the never-translate-site menuitem on a page where
 * translations are active (always-translate-language is enabled).
 * Checking the box on the page automatically restores the page and closes/hides the translations panel.
 */
add_task(
  async function test_panel_closes_on_toggle_never_translate_site_with_translations_active() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
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

    await assertPageIsUntranslated(runInPage);
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

    await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

    await waitForTranslationsPopupEvent("popuphidden", async () => {
      await clickNeverTranslateSite();
    });

    await cleanup();
  }
);

/**
 * Tests the effect of checking the never-translate-site menuitem on a page where
 * translations are inactive (never-translate-language is active).
 * Checking the box on the page automatically closes/hides the translations panel.
 */
add_task(
  async function test_panel_closes_on_toggle_never_translate_site_with_translations_inactive() {
    const { cleanup } = await loadTestPage({
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

    await assertIsNeverTranslateLanguage("es", { checked: false });
    await clickNeverTranslateLanguage();
    await assertIsNeverTranslateLanguage("es", { checked: true });

    await openTranslationsPanel({
      openFromAppMenu: true,
      onOpenPanel: assertPanelDefaultView,
    });
    await openTranslationsSettingsMenu();

    await assertIsNeverTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });
    await waitForTranslationsPopupEvent("popuphidden", async () => {
      await clickNeverTranslateSite();
    });

    await cleanup();
  }
);
