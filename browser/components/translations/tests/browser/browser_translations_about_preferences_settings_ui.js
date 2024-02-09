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

  const {
    backButton,
    header,
    translationsSettingsDescription,
    translateAlwaysHeader,
    translateNeverHeader,
    translateAlwaysAddButton,
    translateNeverAddButton,
    translateNeverSiteHeader,
    translateNeverSiteDesc,
    translateDownloadLanguagesHeader,
    translateDownloadLanguagesLearnMore,
  } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );

  assertVisibility({
    message: "Expect paneTranslations elements to be visible.",
    visible: {
      backButton,
      header,
      translationsSettingsDescription,
      translateAlwaysHeader,
      translateNeverHeader,
      translateAlwaysAddButton,
      translateNeverAddButton,
      translateNeverSiteHeader,
      translateNeverSiteDesc,
      translateDownloadLanguagesHeader,
      translateDownloadLanguagesLearnMore,
    },
    hidden: {
      settingsButton,
    },
  });

  const promise = BrowserTestUtils.waitForEvent(
    document,
    "paneshown",
    false,
    event => event.detail.category === "paneGeneral"
  );

  click(backButton);
  await promise;

  assertVisibility({
    message: "Expect paneGeneral elements to be visible.",
    visible: {
      settingsButton,
    },
    hidden: {
      backButton,
      header,
      translationsSettingsDescription,
      translateAlwaysHeader,
      translateNeverHeader,
      translateAlwaysAddButton,
      translateNeverAddButton,
      translateNeverSiteHeader,
      translateNeverSiteDesc,
      translateDownloadLanguagesHeader,
      translateDownloadLanguagesLearnMore,
    },
  });

  await cleanup();
});
