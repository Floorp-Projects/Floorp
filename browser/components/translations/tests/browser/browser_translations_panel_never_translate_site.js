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
  });

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The translations button is visible."
  );

  await assertPageIsUntranslated(runInPage);

  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });
  await openTranslationsSettingsMenu();

  await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });
  await clickNeverTranslateSite();
  await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

  await assertPageIsUntranslated(runInPage);

  await navigate("Navigate to a Spanish page with the same content principal", {
    url: SPANISH_PAGE_URL_2,
  });

  await assertPageIsUntranslated(runInPage);

  await navigate(
    "Navigate to a Spanish page with a different content principal",
    { url: SPANISH_PAGE_URL_DOT_ORG }
  );

  await assertTranslationsButton(
    { button: true },
    "The translations button should be visible, because this content principal " +
      "has not been denied translations permissions"
  );

  await assertPageIsUntranslated(runInPage);

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
    });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The translations button is visible."
    );

    await assertPageIsUntranslated(runInPage);

    await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });

    await clickTranslateButton({
      downloadHandler: resolveDownloads,
    });

    await assertPageIsTranslated("es", "en", runInPage);

    await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });
    await openTranslationsSettingsMenu();

    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });
    await clickNeverTranslateSite();
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

    await assertPageIsUntranslated(runInPage);

    await navigate("Reload the page", { url: SPANISH_PAGE_URL });

    await assertPageIsUntranslated(runInPage);

    await navigate(
      "Navigate to a Spanish page with the same content principal",
      { url: SPANISH_PAGE_URL_2 }
    );

    await assertPageIsUntranslated(runInPage);

    await navigate(
      "Navigate to a Spanish page with a different content principal",
      { url: SPANISH_PAGE_URL_DOT_ORG }
    );

    await assertTranslationsButton(
      { button: true },
      "The translations button should be visible, because this content principal " +
        "has not been denied translations permissions"
    );

    await assertPageIsUntranslated(runInPage);

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
    });

    await assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
    );

    await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: false });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

    await clickAlwaysTranslateLanguage({
      downloadHandler: resolveDownloads,
    });

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

    await assertPageIsTranslated("es", "en", runInPage);

    await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });
    await openTranslationsSettingsMenu();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: false });

    await clickNeverTranslateSite();

    await assertIsAlwaysTranslateLanguage("es", { checked: true });
    await assertIsNeverTranslateSite(SPANISH_PAGE_URL, { checked: true });

    await assertPageIsUntranslated(runInPage);

    await navigate("Reload the page", { url: SPANISH_PAGE_URL });

    await assertPageIsUntranslated(runInPage);

    await navigate(
      "Navigate to a Spanish page with the same content principal",
      { url: SPANISH_PAGE_URL_2 }
    );

    await assertPageIsUntranslated(runInPage);

    await navigate(
      "Navigate to a Spanish page with a different content principal",
      {
        url: SPANISH_PAGE_URL_DOT_ORG,
        downloadHandler: resolveDownloads,
      }
    );

    await assertPageIsTranslated("es", "en", runInPage);

    await cleanup();
  }
);
