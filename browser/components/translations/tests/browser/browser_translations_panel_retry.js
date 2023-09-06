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

  await assertPageIsUntranslated(runInPage);

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

  await assertPageIsTranslated("es", "en", runInPage);

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

  await assertPageIsTranslated("es", "fr", runInPage);

  await cleanup();
});
