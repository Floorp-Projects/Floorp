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
    translateAlwaysMenuList,
    translateNeverMenuList,
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
      translateAlwaysMenuList,
      translateNeverMenuList,
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
      translateAlwaysMenuList,
      translateNeverMenuList,
      translateNeverSiteHeader,
      translateNeverSiteDesc,
      translateDownloadLanguagesHeader,
      translateDownloadLanguagesLearnMore,
    },
  });
  await cleanup();
});

add_task(async function test_translations_settings_always_translate() {
  const {
    cleanup,
    elements: { settingsButton },
  } = await setupAboutPreferences(LANGUAGE_PAIRS, {
    prefs: [["browser.translations.newSettingsUI.enable", true]],
  });

  const document = gBrowser.selectedBrowser.contentDocument;

  assertVisibility({
    message: "Expect paneGeneral elements to be visible.",
    visible: { settingsButton },
  });

  const { translateAlwaysMenuList } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );
  let alwaysTranslateSection = document.getElementById(
    "translations-settings-always-translate-section"
  );
  await testLanguageList(alwaysTranslateSection, translateAlwaysMenuList);

  await cleanup();
});

async function testLanguageList(translateSection, menuList) {
  const sectionName =
    translateSection.id === "translations-settings-always-translate-section"
      ? "Always"
      : "Never";

  is(
    translateSection.querySelector(".translations-settings-languages-card"),
    null,
    `Language list not present in ${sectionName} Translate list`
  );

  for (let i = 0; i < menuList.children[0].children.length; i++) {
    menuList.value = menuList.children[0].children[i].value;

    let clickMenu = BrowserTestUtils.waitForEvent(menuList, "command");
    menuList.dispatchEvent(new Event("command"));
    await clickMenu;

    /** Languages are always added on the top, so check the firstChild
     * for newly added languages.
     * the firstChild.lastChild.innerText is the language display name
     * which is compared with the menulist display name that is selected
     */
    is(
      translateSection.querySelector(".translations-settings-language-list")
        .firstChild.lastChild.innerText,
      getIntlDisplayName(menuList.children[0].children[i].value),
      `Language list has element ${getIntlDisplayName(
        menuList.children[0].children[i].value
      )}`
    );
  }
  /** The test cases has 4 languages, so check if 4 languages are added to the list */
  let langNum = translateSection.querySelector(
    ".translations-settings-language-list"
  ).childElementCount;
  is(langNum, 4, "Number of languages added is 4");

  const languagelist = translateSection.querySelector(
    ".translations-settings-language-list"
  );

  for (let i = 0; i < langNum; i++) {
    // Delete the first language in the list
    let langName = languagelist.children[0].lastChild.innerText;
    let langButton = languagelist.children[0].querySelector("moz-button");

    let clickButton = BrowserTestUtils.waitForEvent(langButton, "click");
    langButton.click();
    await clickButton;

    if (i < langNum - 1) {
      is(
        languagelist.childElementCount,
        langNum - i - 1,
        `${langName} removed from ${sectionName}  Translate`
      );
    } else {
      /** Check if the language list card is removed after removing the last language */
      is(
        translateSection.querySelector(".translations-settings-languages-card"),
        null,
        `${langName} removed from ${sectionName} Translate`
      );
    }
  }
}

add_task(async function test_translations_settings_never_translate() {
  const {
    cleanup,
    elements: { settingsButton },
  } = await setupAboutPreferences(LANGUAGE_PAIRS, {
    prefs: [["browser.translations.newSettingsUI.enable", true]],
  });

  const document = gBrowser.selectedBrowser.contentDocument;

  assertVisibility({
    message: "Expect paneGeneral elements to be visible.",
    visible: { settingsButton },
  });

  const { translateNeverMenuList } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );
  let neverTranslateSection = document.getElementById(
    "translations-settings-never-translate-section"
  );
  await testLanguageList(neverTranslateSection, translateNeverMenuList);
  await cleanup();
});

add_task(async function test_translations_settings_download_languages() {
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

  const { translateDownloadLanguagesList } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );

  let langList = translateDownloadLanguagesList.querySelector(
    ".translations-settings-language-list"
  );

  for (let i = 0; i < langList.children.length; i++) {
    is(
      langList.children[i]
        .querySelector("moz-button")
        .classList.contains("translations-settings-download-icon"),
      true,
      "Download icon is visible"
    );

    let clickButton = BrowserTestUtils.waitForEvent(
      langList.children[i].querySelector("moz-button"),
      "click"
    );
    langList.children[i].querySelector("moz-button").click();
    await clickButton;

    is(
      langList.children[i]
        .querySelector("moz-button")
        .classList.contains("translations-settings-delete-icon"),
      true,
      "Delete icon is visible"
    );

    clickButton = BrowserTestUtils.waitForEvent(
      langList.children[i].querySelector("moz-button"),
      "click"
    );
    langList.children[i].querySelector("moz-button").click();
    await clickButton;

    is(
      langList.children[i]
        .querySelector("moz-button")
        .classList.contains("translations-settings-download-icon"),
      true,
      "Download icon is visible"
    );
  }
  await cleanup();
});
