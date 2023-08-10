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

  const { button } = await assertTranslationsButton(
    { button: true },
    "The button is available."
  );

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is in Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(button, "Opening the popup");
    },
    assertPanelDefaultView
  );

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-translate-button"),
      "Start translating by clicking the translate button."
    );
  });

  await resolveDownloads(1);

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The pages H1 is translated.",
      getH1,
      "DON QUIJOTE DE LA MANCHA [es to en, html]"
    );
  });

  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      click(button, "Re-opening the popup");
    },
    assertPanelRevisitView
  );

  info('Switch to language to "fr"');
  const toSelect = getById("translations-panel-to");
  toSelect.value = "fr";
  toSelect.dispatchEvent(new Event("command"));

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-translate-button"),
      "Re-translate the page by clicking the translate button."
    );
  });

  // This is a pivot language which requires 2 models.
  await resolveDownloads(2);

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The pages H1 is translated using the changed languages.",
      getH1,
      "DON QUIJOTE DE LA MANCHA [es to fr, html]"
    );
  });

  await cleanup();
});
