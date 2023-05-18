/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const languagePairs = [
  { fromLang: "es", toLang: "en", isBeta: false },
  { fromLang: "en", toLang: "es", isBeta: false },
  { fromLang: "fr", toLang: "en", isBeta: false },
  { fromLang: "en", toLang: "fr", isBeta: false },
  { fromLang: "en", toLang: "uk", isBeta: true },
  { fromLang: "uk", toLang: "en", isBeta: true },
];

const spanishPageUrl = TRANSLATIONS_TESTER_ES;
const englishPageUrl = TRANSLATIONS_TESTER_EN;

/**
 * Test that the translations button is correctly visible when navigating between pages.
 */
add_task(async function test_button_visible_navigation() {
  info("Start at a page in Spanish.");
  const { cleanup } = await loadTestPage({
    page: spanishPageUrl,
    languagePairs,
  });

  await assertTranslationsButton(
    button => !button.hidden,
    "The button should be visible since the page can be translated from Spanish."
  );

  navigate(englishPageUrl, "Navigate to an English page.");

  await assertTranslationsButton(
    button => button.hidden,
    "The button should be invisible since the page is in English."
  );

  navigate(spanishPageUrl, "Navigate back to a Spanish page.");

  await assertTranslationsButton(
    button => !button.hidden,
    "The button should be visible again since the page is in Spanish."
  );

  await cleanup();
});

/**
 * Test that the translations button is correctly visible when opening and switch tabs.
 */
add_task(async function test_button_visible() {
  info("Start at a page in Spanish.");

  const { cleanup, tab: spanishTab } = await loadTestPage({
    page: spanishPageUrl,
    languagePairs,
  });

  await assertTranslationsButton(
    button => !button.hidden,
    "The button should be visible since the page can be translated from Spanish."
  );

  const { removeTab, tab: englishTab } = await addTab(
    englishPageUrl,
    "Creating a new tab for a page in English."
  );

  await assertTranslationsButton(
    button => button.hidden,
    "The button should be invisible since the tab is in English."
  );

  await switchTab(spanishTab);

  await assertTranslationsButton(
    button => !button.hidden,
    "The button should be visible again since the page is in Spanish."
  );

  await switchTab(englishTab);

  await assertTranslationsButton(
    button => button.hidden,
    "Don't show for english pages"
  );

  await removeTab();
  await cleanup();
});

/**
 * Tests a basic panel open, translation, and restoration to the original language.
 */
add_task(async function test_translations_panel_basics() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: spanishPageUrl,
    languagePairs,
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

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-default-translate-button"),
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

  await waitForTranslationsPopupEvent("popupshown", () => {
    click(button, "Re-opening the popup");
  });

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-revisit-restore-button"),
      "Click the restore language button."
    );
  });

  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is restored to Spanish.",
      getH1,
      "Don Quijote de La Mancha"
    );
  });

  await cleanup();
});

/**
 * Tests translating, and then immediately translating to a new language.
 */
add_task(async function test_translations_panel_retry() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: spanishPageUrl,
    languagePairs,
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

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-default-translate-button"),
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

  await waitForTranslationsPopupEvent("popupshown", () => {
    click(button, "Re-opening the popup");
  });

  info('Switch to language to "fr"');
  const toSelect = getById("translations-panel-revisit-to");
  toSelect.value = "fr";
  toSelect.dispatchEvent(new Event("command"));

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-revisit-translate-button"),
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

/**
 * Tests switching the language.
 */
add_task(async function test_translations_panel_switch_language() {
  const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
    page: spanishPageUrl,
    languagePairs,
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

/**
 * Tests a panel open, and hitting the cancel button.
 */
add_task(async function test_translations_panel_cancel() {
  const { cleanup } = await loadTestPage({
    page: spanishPageUrl,
    languagePairs,
  });

  const button = await assertTranslationsButton(
    b => !b.hidden,
    "The button is available."
  );

  await waitForTranslationsPopupEvent("popupshown", () => {
    click(button, "Opening the popup");
  });

  await waitForTranslationsPopupEvent("popuphidden", () => {
    click(
      getByL10nId("translations-panel-default-translate-cancel"),
      "Click the cancel button."
    );
  });

  await cleanup();
});

/**
 * Tests that languages are displayed correctly as being in beta or not.
 */
add_task(async function test_translations_panel_display_beta_languages() {
  const { cleanup } = await loadTestPage({
    page: spanishPageUrl,
    languagePairs,
  });

  function assertBetaDisplay(selectElement) {
    const betaL10nId = "translations-panel-displayname-beta";
    const options = selectElement.firstChild.getElementsByTagName("menuitem");
    for (const option of options) {
      for (const languagePair of languagePairs) {
        if (
          languagePair.fromLang === option.value ||
          languagePair.toLang === option.value
        ) {
          if (option.getAttribute("data-l10n-id") === betaL10nId) {
            is(
              languagePair.isBeta,
              true,
              `Since data-l10n-id was ${betaL10nId} for ${option.value}, then it must be part of a beta language pair, but it was not.`
            );
          }
          if (!languagePair.isBeta) {
            is(
              option.getAttribute("data-l10n-id") === betaL10nId,
              false,
              `Since the languagePair is non-beta, the language option ${option.value} should not have a data-l10-id of ${betaL10nId}, but it does.`
            );
          }
        }
      }
    }
  }

  const fromSelect = document.getElementById("translations-panel-dual-from");
  const toSelect = document.getElementById("translations-panel-dual-to");

  assertBetaDisplay(fromSelect);
  assertBetaDisplay(toSelect);

  await cleanup();
});

/**
 * Test managing the languages menu item.
 */
add_task(async function test_translations_panel_manage_languages() {
  const { cleanup } = await loadTestPage({
    page: spanishPageUrl,
    languagePairs,
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
