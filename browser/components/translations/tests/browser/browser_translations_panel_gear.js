/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test managing the languages menu item.
 */
add_task(async function test_translations_panel_manage_languages() {
  const { cleanup } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  const button = await assertTranslationsButton(
    b => !b.hidden,
    "The button is available."
  );

  await waitForTranslationsPopupEvent("popupshown", () => {
    click(button, "Opening the popup");
  });

  const gearIcon = getByL10nId("translations-panel-settings-button");
  click(gearIcon, "Open the preferences menu");

  const manageLanguages = getByL10nId(
    "translations-panel-settings-manage-languages"
  );
  info("Choose to manage the languages.");
  manageLanguages.doCommand();

  await TestUtils.waitForCondition(
    () => gBrowser.currentURI.spec === "about:preferences#general",
    "Waiting for about:preferences to be opened."
  );

  info("Remove the about:preferences tab");
  gBrowser.removeCurrentTab();

  await cleanup();
});

/**
 * Tests switching the language.
 */
add_task(async function test_translations_panel_switch_language() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  const button = await assertTranslationsButton(
    b => !b.hidden,
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

  await waitForTranslationsPopupEvent("popupshown", () => {
    click(button, "Opening the popup");
  });

  const gearIcon = getByL10nId("translations-panel-settings-button");
  click(gearIcon, "Open the preferences menu");

  await waitForViewShown(() => {
    info("Switch to choose language view");
    getByL10nId(
      "translations-panel-settings-change-source-language"
    ).doCommand();
  });

  const translateButton = getByL10nId(
    "translations-panel-default-translate-button"
  );
  const fromSelect = getById("translations-panel-dual-from");
  const toSelect = getById("translations-panel-dual-to");

  ok(translateButton.disabled, "The translate button starts as disabled");

  info('Switch from language to "en"');
  fromSelect.value = "en";
  fromSelect.dispatchEvent(new Event("command"));

  info('Switch to language to "fr"');
  toSelect.value = "fr";
  toSelect.dispatchEvent(new Event("command"));

  ok(!translateButton.disabled, "The translate button can now be used");

  info('Switch to language to "en"');
  toSelect.value = "en";
  toSelect.dispatchEvent(new Event("command"));

  ok(
    translateButton.disabled,
    "Choosing to translate to and from English causes the translate button to be disabled again."
  );

  info('Switch to language back to "fr"');
  toSelect.value = "fr";
  toSelect.dispatchEvent(new Event("command"));

  ok(!translateButton.disabled, "The translate button can be used again.");

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      translateButton,
      "Start translating by clicking the translate button."
    );
  });

  await resolveDownloads(1);

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The pages H1 is translated using the changed languages.",
      getH1,
      "DON QUIJOTE DE LA MANCHA [en to fr, html]"
    );
  });

  await cleanup();
});
