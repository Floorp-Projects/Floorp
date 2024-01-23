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

  await FullPageTranslationsTestUtils.assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The translations button is visible."
  );

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await FullPageTranslationsTestUtils.openTranslationsPanel({
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
  });
  await FullPageTranslationsTestUtils.openTranslationsSettingsMenu();

  await FullPageTranslationsTestUtils.assertIsNeverTranslateSite(
    SPANISH_PAGE_URL,
    { checked: false }
  );
  await FullPageTranslationsTestUtils.clickNeverTranslateSite();
  await FullPageTranslationsTestUtils.assertIsNeverTranslateSite(
    SPANISH_PAGE_URL,
    { checked: true }
  );

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await navigate("Navigate to a Spanish page with the same content principal", {
    url: SPANISH_PAGE_URL_2,
  });

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await navigate(
    "Navigate to a Spanish page with a different content principal",
    { url: SPANISH_PAGE_URL_DOT_ORG }
  );

  await FullPageTranslationsTestUtils.assertTranslationsButton(
    { button: true },
    "The translations button should be visible, because this content principal " +
      "has not been denied translations permissions"
  );

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await cleanup();
});
