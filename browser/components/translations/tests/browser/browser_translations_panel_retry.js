/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests translating, and then immediately translating to a new language.
 */
add_task(async function test_translations_panel_retry() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  await assertTranslationsButton({ button: true }, "The button is available.");

  await assertPageIsUntranslated(runInPage);

  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });

  await clickTranslateButton({
    downloadHandler: resolveDownloads,
  });

  await assertPageIsTranslated("es", "en", runInPage);

  await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });

  switchSelectedToLanguage("fr");

  await clickTranslateButton({
    downloadHandler: resolveDownloads,
    pivotTranslation: true,
  });

  await assertPageIsTranslated("es", "fr", runInPage);

  await cleanup();
});
