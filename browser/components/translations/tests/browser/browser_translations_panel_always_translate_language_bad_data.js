/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that having an "always translate" set to your app locale doesn't break things.
 */
add_task(async function test_always_translate_with_bad_data() {
  const { cleanup, runInPage } = await loadTestPage({
    page: ENGLISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
    prefs: [["browser.translations.alwaysTranslateLanguages", "en,fr"]],
  });

  await openTranslationsPanel({
    onOpenPanel: assertPanelDefaultView,
    openFromAppMenu: true,
  });
  await openTranslationsSettingsMenu();

  await assertIsAlwaysTranslateLanguage("en", {
    checked: false,
    disabled: true,
  });
  await closeSettingsMenuIfOpen();
  await closeTranslationsPanelIfOpen();

  info("Checking that the page is untranslated");
  await runInPage(async TranslationsTest => {
    const { getH1 } = TranslationsTest.getSelectors();
    await TranslationsTest.assertTranslationResult(
      "The page's H1 is untranslated and in the original English.",
      getH1,
      '"The Wonderful Wizard of Oz" by L. Frank Baum'
    );
  });

  await cleanup();
});

//------------------------------------------------------------------------------
// The following functions were pasted in from Fx119 for an uplift of this test.
// browser/components/translations/tests/browser/head.js

async function openTranslationsPanelViaTranslationsButton({
  onOpenPanel = null,
  openWithKeyboard = false,
}) {
  info("Opening the translations panel via the translations button");
  const { button } = await assertTranslationsButton(
    { button: true },
    "The translations button is visible."
  );
  await waitForTranslationsPopupEvent(
    "popupshown",
    () => {
      if (openWithKeyboard) {
        hitEnterKey(button, "Opening the popup with keyboard");
      } else {
        click(button, "Opening the popup");
      }
    },
    onOpenPanel
  );
}

async function openTranslationsPanel({
  onOpenPanel = null,
  openFromAppMenu = false,
  openWithKeyboard = false,
}) {
  await closeTranslationsPanelIfOpen();
  if (openFromAppMenu) {
    await openTranslationsPanelViaAppMenu({ onOpenPanel, openWithKeyboard });
  } else {
    await openTranslationsPanelViaTranslationsButton({
      onOpenPanel,
      openWithKeyboard,
    });
  }
}

async function openTranslationsSettingsMenu() {
  info("Opening the translations panel settings menu");
  const gearIcons = getAllByL10nId("translations-panel-settings-button");
  for (const gearIcon of gearIcons) {
    if (gearIcon.hidden) {
      continue;
    }
    click(gearIcon, "Open the settings menu");
    info("Waiting for settings menu to open.");
    const manageLanguages = await waitForCondition(() =>
      maybeGetByL10nId("translations-panel-settings-manage-languages")
    );
    ok(
      manageLanguages,
      "The manage languages item should be visible in the settings menu."
    );
    return;
  }
}
