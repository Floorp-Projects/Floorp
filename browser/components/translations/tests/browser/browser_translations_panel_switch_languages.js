/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests switching the language.
 */
add_task(async function test_translations_panel_switch_language() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  await FullPageTranslationsTestUtils.assertTranslationsButton(
    { button: true },
    "The button is available."
  );

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await FullPageTranslationsTestUtils.openTranslationsPanel({
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
  });

  const { translateButton } = TranslationsPanel.elements;

  ok(!translateButton.disabled, "The translate button starts as enabled");

  FullPageTranslationsTestUtils.assertSelectedFromLanguage("es");
  FullPageTranslationsTestUtils.assertSelectedToLanguage("en");

  FullPageTranslationsTestUtils.switchSelectedFromLanguage("en");

  ok(
    translateButton.disabled,
    "The translate button is disabled when the languages are the same"
  );

  FullPageTranslationsTestUtils.switchSelectedFromLanguage("es");

  ok(
    !translateButton.disabled,
    "When the languages are different it can be translated"
  );

  FullPageTranslationsTestUtils.switchSelectedFromLanguage("");

  ok(
    translateButton.disabled,
    "The translate button is disabled nothing is selected."
  );

  FullPageTranslationsTestUtils.switchSelectedFromLanguage("en");
  FullPageTranslationsTestUtils.switchSelectedToLanguage("fr");

  ok(!translateButton.disabled, "The translate button can now be used");

  await FullPageTranslationsTestUtils.clickTranslateButton({
    downloadHandler: resolveDownloads,
  });

  await FullPageTranslationsTestUtils.assertPageIsTranslated(
    "en",
    "fr",
    runInPage
  );

  await cleanup();
});
