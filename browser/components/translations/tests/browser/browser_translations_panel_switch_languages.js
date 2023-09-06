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

  const translateButton = getByL10nId("translations-panel-translate-button");
  const fromSelect = getById("translations-panel-from");
  const toSelect = getById("translations-panel-to");

  ok(!translateButton.disabled, "The translate button starts as enabled");
  is(fromSelect.value, "es", "The from select starts as Spanish");
  is(toSelect.value, "en", "The to select starts as English");

  info('Switch from language to "es"');
  fromSelect.value = "en";
  fromSelect.dispatchEvent(new Event("command"));

  ok(
    translateButton.disabled,
    "The translate button is disabled when the languages are the same"
  );

  info('Switch from language back to "es"');
  fromSelect.value = "es";
  fromSelect.dispatchEvent(new Event("command"));

  ok(
    !translateButton.disabled,
    "When the languages are different it can be translated"
  );

  info("Switch to language to nothing");
  fromSelect.value = "";
  fromSelect.dispatchEvent(new Event("command"));

  ok(
    translateButton.disabled,
    "The translate button is disabled nothing is selected."
  );

  info('Switch from language to "en"');
  fromSelect.value = "en";
  fromSelect.dispatchEvent(new Event("command"));

  info('Switch to language to "fr"');
  toSelect.value = "fr";
  toSelect.dispatchEvent(new Event("command"));

  ok(!translateButton.disabled, "The translate button can now be used");

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      translateButton,
      "Start translating by clicking the translate button."
    );
  });

  await resolveDownloads(1);

  await assertPageIsTranslated("en", "fr", runInPage);

  await cleanup();
});
