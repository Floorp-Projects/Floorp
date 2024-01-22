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
  ok(
    BrowserTestUtils.isVisible(settingsButton),
    "Expected the translations settings button to be visible"
  );

  const { backButton, header } =
    await openAboutPreferencesTranslationsSettingsPane(settingsButton);

  ok(
    BrowserTestUtils.isHidden(settingsButton),
    "Translations settings, Settings Button is hidden"
  );
  ok(
    BrowserTestUtils.isVisible(header),
    "Translations settings Header is visible"
  );
  ok(
    BrowserTestUtils.isVisible(backButton),
    "Translations settings Back Button is visible"
  );

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
