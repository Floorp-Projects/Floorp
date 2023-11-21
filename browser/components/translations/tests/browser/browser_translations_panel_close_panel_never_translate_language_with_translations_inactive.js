"use strict";

/**
 * Tests the effect of checking the never-translate-language menuitem on a page where
 * translations and never translate site are inactive.
 * Checking the box on the page automatically closes/hides the translations panel.
 */
add_task(async function test_panel_closes_on_toggle_never_translate_language() {
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
  await waitForTranslationsPopupEvent("popuphidden", async () => {
    await clickNeverTranslateLanguage();
  });
  await cleanup();
});

/**
 * Tests the effect of checking the never-translate-language menuitem on a page where
 * translations are inactive (never-translate-site is enabled).
 * Checking the box on the page automatically closes/hides the translations panel.
 */
add_task(
  async function test_panel_closes_on_toggle_never_translate_language_with_never_translate_site_enabled() {
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
    await clickNeverTranslateSite();
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

    await openTranslationsPanel({
      openFromAppMenu: true,
      onOpenPanel: assertPanelDefaultView,
    });
    await openTranslationsSettingsMenu();
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

    await assertIsNeverTranslateLanguage("es", { checked: false });
    await waitForTranslationsPopupEvent("popuphidden", async () => {
      await clickNeverTranslateLanguage();
    });
    await cleanup();
  }
);
