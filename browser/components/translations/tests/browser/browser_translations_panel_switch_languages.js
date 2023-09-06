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

  await assertTranslationsButton({ button: true }, "The button is available.");

  await assertPageIsUntranslated(runInPage);

  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });

  const { translateButton } = TranslationsPanel.elements;

  ok(!translateButton.disabled, "The translate button starts as enabled");

  assertSelectedFromLanguage("es");
  assertSelectedToLanguage("en");

  switchSelectedFromLanguage("en");

  ok(
    translateButton.disabled,
    "The translate button is disabled when the languages are the same"
  );

  switchSelectedFromLanguage("es");

  ok(
    !translateButton.disabled,
    "When the languages are different it can be translated"
  );

  switchSelectedFromLanguage("");

  ok(
    translateButton.disabled,
    "The translate button is disabled nothing is selected."
  );

  switchSelectedFromLanguage("en");
  switchSelectedToLanguage("fr");

  ok(!translateButton.disabled, "The translate button can now be used");

  await clickTranslateButton({
    downloadHandler: resolveDownloads,
  });

  await assertPageIsTranslated("en", "fr", runInPage);

  await cleanup();
});
