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
  });

  await FullPageTranslationsTestUtils.assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The translations button is visible."
  );

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await FullPageTranslationsTestUtils.openTranslationsPanel({
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
  });
  await FullPageTranslationsTestUtils.openTranslationsSettingsMenu();

  await FullPageTranslationsTestUtils.assertIsNeverTranslateLanguage("es", {
    checked: false,
  });
  await FullPageTranslationsTestUtils.clickNeverTranslateLanguage();
  await FullPageTranslationsTestUtils.assertIsNeverTranslateLanguage("es", {
    checked: true,
  });

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await navigate("Reload the page", { url: SPANISH_PAGE_URL });

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await navigate("Navigate to a different Spanish page", {
    url: SPANISH_PAGE_URL_DOT_ORG,
  });

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

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
    });

    await FullPageTranslationsTestUtils.assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
    );

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await FullPageTranslationsTestUtils.openTranslationsPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
    });

    await FullPageTranslationsTestUtils.clickTranslateButton({
      downloadHandler: resolveDownloads,
    });

    await FullPageTranslationsTestUtils.assertPageIsTranslated(
      "es",
      "en",
      runInPage
    );

    await FullPageTranslationsTestUtils.openTranslationsPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewRevisit,
    });
    await FullPageTranslationsTestUtils.openTranslationsSettingsMenu();

    await FullPageTranslationsTestUtils.assertIsNeverTranslateLanguage("es", {
      checked: false,
    });
    await FullPageTranslationsTestUtils.clickNeverTranslateLanguage();
    await FullPageTranslationsTestUtils.assertIsNeverTranslateLanguage("es", {
      checked: true,
    });

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await navigate("Reload the page", { url: SPANISH_PAGE_URL });

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

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
    });

    await FullPageTranslationsTestUtils.assertTranslationsButton(
      { button: true, circleArrows: false, locale: false, icon: true },
      "The button is available."
    );

    await FullPageTranslationsTestUtils.openTranslationsPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
    });
    await FullPageTranslationsTestUtils.openTranslationsSettingsMenu();

    await FullPageTranslationsTestUtils.assertIsAlwaysTranslateLanguage("es", {
      checked: false,
    });
    await FullPageTranslationsTestUtils.assertIsNeverTranslateLanguage("es", {
      checked: false,
    });

    await FullPageTranslationsTestUtils.clickAlwaysTranslateLanguage({
      downloadHandler: resolveDownloads,
    });

    await FullPageTranslationsTestUtils.assertIsAlwaysTranslateLanguage("es", {
      checked: true,
    });
    await FullPageTranslationsTestUtils.assertIsNeverTranslateLanguage("es", {
      checked: false,
    });

    await FullPageTranslationsTestUtils.assertPageIsTranslated(
      "es",
      "en",
      runInPage
    );

    await FullPageTranslationsTestUtils.openTranslationsPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewRevisit,
    });
    await FullPageTranslationsTestUtils.openTranslationsSettingsMenu();

    await FullPageTranslationsTestUtils.assertIsAlwaysTranslateLanguage("es", {
      checked: true,
    });
    await FullPageTranslationsTestUtils.assertIsNeverTranslateLanguage("es", {
      checked: false,
    });

    await FullPageTranslationsTestUtils.clickNeverTranslateLanguage();

    await FullPageTranslationsTestUtils.assertIsAlwaysTranslateLanguage("es", {
      checked: false,
    });
    await FullPageTranslationsTestUtils.assertIsNeverTranslateLanguage("es", {
      checked: true,
    });

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await navigate("Reload the page", { url: SPANISH_PAGE_URL });

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await cleanup();
  }
);
