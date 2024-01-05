/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests a basic panel open, translation, and restoration to the original language.
 */
add_task(async function test_translations_panel_basics() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  const { button } = await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The button is available."
  );

  is(button.getAttribute("data-l10n-id"), "urlbar-translations-button2");

  await assertPageIsUntranslated(runInPage);

  await openTranslationsPanel({ onOpenPanel: assertPanelDefaultView });

  const panel = document.getElementById("translations-panel");
  const label = document.getElementById(panel.getAttribute("aria-labelledby"));
  ok(label, "The a11y label for the panel can be found.");
  assertIsVisible(true, { element: label });

  await clickTranslateButton();

  await assertTranslationsButton(
    { button: true, circleArrows: true, locale: false, icon: true },
    "The icon presents the loading indicator."
  );

  await openTranslationsPanel({ onOpenPanel: assertPanelLoadingView });

  await clickCancelButton();

  await resolveDownloads(1);

  await assertPageIsTranslated("es", "en", runInPage);

  await openTranslationsPanel({ onOpenPanel: assertPanelRevisitView });

  await clickRestoreButton();

  await assertPageIsUntranslated(runInPage);

  await assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The button is reverted to have an icon."
  );

  await cleanup();
});
