/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_translations_settings_pane_elements() {
  const {
    cleanup,
    elements: { settingsButton },
  } = await setupAboutPreferences(LANGUAGE_PAIRS, {
    prefs: [["browser.translations.newSettingsUI.enable", true]],
  });

  assertVisibility({
    message: "Expect paneGeneral elements to be visible.",
    visible: { settingsButton },
  });

  const { backButton, header } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );

  assertVisibility({
    message: "Expect paneTranslations elements to be visible.",
    visible: { backButton, header },
    hidden: { settingsButton },
  });

  is(
    header.getAttribute("data-l10n-id"),
    "translations-settings-header-text",
    "Translations settings header is localized"
  );
  is(
    backButton.getAttribute("data-l10n-id"),
    "translations-settings-back-button-text",
    "Translations settings back button is localized"
  );

  const promise = BrowserTestUtils.waitForEvent(
    document,
    "paneshown",
    false,
    event => event.detail.category === "paneGeneral"
  );

  click(backButton);
  await promise;

  ok(
    BrowserTestUtils.isHidden(backButton),
    "Translations settings Back Button is hidden"
  );

  await cleanup();
});
