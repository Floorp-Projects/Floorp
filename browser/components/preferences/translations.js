/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

ChromeUtils.defineESModuleGetters(this, {
  TranslationsParent: "resource://gre/actors/TranslationsParent.sys.mjs",
});

let gTranslationsPane = {
  init() {
    document
      .getElementById("translations-settings-back-button")
      .addEventListener("click", function () {
        gotoPref("general");
      });

    document
      .getElementById("translations-settings-always-translate-list")
      .addEventListener("command", this.addAlwaysLanguage);

    document
      .getElementById("translations-settings-never-translate-list")
      .addEventListener("command", this.addNeverLanguage);

    this.buildLanguageDropDowns();
    this.buildDownloadLanguageList();
  },

  /**
   * Populate the Drop down list with the list of supported languages
   * for the user to choose languages to add to Always translate and
   * Never translate settings list.
   */
  async buildLanguageDropDowns() {
    const { fromLanguages } = await TranslationsParent.getSupportedLanguages();
    let alwaysLangPopup = document.getElementById(
      "translations-settings-always-translate-popup"
    );
    let neverLangPopup = document.getElementById(
      "translations-settings-never-translate-popup"
    );

    for (const { langTag, displayName } of fromLanguages) {
      const alwaysLang = document.createXULElement("menuitem");
      alwaysLang.setAttribute("value", langTag);
      alwaysLang.setAttribute("label", displayName);
      alwaysLangPopup.appendChild(alwaysLang);
      const neverLang = document.createXULElement("menuitem");
      neverLang.setAttribute("value", langTag);
      neverLang.setAttribute("label", displayName);
      neverLangPopup.appendChild(neverLang);
    }
  },

  /**
   * Show a list of languages for the user to be able to install
   * and uninstall language models for local translation.
   */
  async buildDownloadLanguageList() {
    const supportedLanguages = await TranslationsParent.getSupportedLanguages();
    const languageList = TranslationsParent.getLanguageList(supportedLanguages);

    let installList = document.querySelector(
      ".translations-settings-language-list"
    );

    // The option to download "All languages" is added in xhtml.
    // Here the option to download individual languages is dynamically added
    // based on the supported language list
    installList
      .querySelector("moz-button")
      .addEventListener("click", installLanguage);

    for (const language of languageList) {
      const languageElement = document.createElement("div");
      languageElement.classList.add("translations-settings-language");

      const languageLabel = document.createElement("label");
      languageLabel.textContent = language.displayName;
      languageLabel.setAttribute("value", language.langTag);
      // Using the language tag suffix to create unique id for each language
      languageLabel.id = "translations-settings-download-" + language.langTag;

      const installButton = document.createElement("moz-button");
      installButton.classList.add("translations-settings-download-icon");
      installButton.setAttribute("type", "ghost icon");
      installButton.addEventListener("click", installLanguage);
      installButton.setAttribute("aria-label", languageLabel.id);

      languageElement.appendChild(installButton);
      languageElement.appendChild(languageLabel);
      installList.appendChild(languageElement);
    }
  },

  /**
   * Event handler when the user wants to add a language to
   * Always translate settings list.
   */
  addAlwaysLanguage(event) {
    /* TODO:
    The function addLanguage adds the HTML element.
    It will be moved to the observer in the next Bug - 1881259 .
    It is here just to test the UI.
    For now a language can be added multiple times.

    In the next bug we will maintain a local state of preferences.
    When a language is added or removed, the user event updates the preferences in the Services,
    This triggers the observer which compares the preferences in the local state and the
    preferences in the Services and adds or removes the language in the local state and updates the
     UI to reflect the updated Preferences in the Services.
    */
    addLanguage(
      event,
      "translations-settings-always-translate-section",
      deleteAlwaysLanguage
    );
  },

  /**
   * Event handler when the user wants to add a language to
   * Never translate settings list.
   */
  addNeverLanguage(event) {
    /* TODO:
    The function addLanguage adds the HTML element.
    It will be moved to the observer in the next Bug - 1881259 .
    It is here just to test the UI.
    For now a language can be added multiple times.

    In the next bug we will maintain a local state of preferences.
    When a language is added or removed, the user event updates the preferences in the Services,
    This triggers the observer which compares the preferences in the local state and the
    preferences in the Services and adds or removes the language in the local state and updates the
     UI to reflect the updated Preferences in the Services.
    */
    addLanguage(
      event,
      "translations-settings-never-translate-section",
      deleteNeverLanguage
    );
  },
};

/**
 * Function to add a language selected by the user to the list of
 * Always/Never translate settings list.
 */
async function addLanguage(event, listClass, delHandler) {
  const translatePrefix =
    listClass === "translations-settings-never-translate-section"
      ? "never"
      : "always";
  let translateSection = document.getElementById(listClass);
  let languageList = translateSection.querySelector(
    ".translations-settings-language-list"
  );

  // While adding the first language, add the Header and language List div
  if (!languageList) {
    let languageCard = document.createElement("div");
    languageCard.classList.add("translations-settings-languages-card");
    translateSection.appendChild(languageCard);

    let languageHeader = document.createElement("h3");
    languageCard.appendChild(languageHeader);
    languageHeader.setAttribute(
      "data-l10n-id",
      "translations-settings-language-header"
    );
    languageHeader.classList.add("translations-settings-language-header");

    languageList = document.createElement("div");
    languageList.classList.add("translations-settings-language-list");
    languageCard.appendChild(languageList);
  }
  const languageElement = document.createElement("div");
  languageElement.classList.add("translations-settings-language");
  // Add the language after the Language Header
  languageList.insertBefore(languageElement, languageList.firstChild);

  const languageLabel = document.createElement("label");
  languageLabel.textContent = event.target.getAttribute("label");
  languageLabel.setAttribute("value", event.target.getAttribute("value"));
  // Using the language tag suffix to create unique id for each language
  // add prefix for the always/never translate
  languageLabel.id =
    "translations-settings-language-" +
    translatePrefix +
    "-" +
    event.target.getAttribute("value");

  const delButton = document.createElement("moz-button");
  delButton.classList.add("translations-settings-delete-icon");
  delButton.setAttribute("type", "ghost icon");
  delButton.addEventListener("click", delHandler);
  delButton.setAttribute("aria-label", languageLabel.id);

  languageElement.appendChild(delButton);
  languageElement.appendChild(languageLabel);

  /*  After a language is selected the menulist button display will be set to the
      selected langauge. After processing the button event the
      data-l10n-id of the menulist button is restored to "Add Language" */
  const menuList = translateSection.querySelector("menulist");
  await document.l10n.translateElements([menuList]);
}

/**
 * Event Handler to delete a language selected by the user from the list of
 * Always translate settings list.
 */
function deleteAlwaysLanguage(event) {
  /* TODO:
  The function removeLanguage removes the HTML element.
  It will be moved to the observer in the next Bug - 1881259 .
  It is here just to test the UI.

  In the next bug we will maintain a local state of preferences.
  When a language is added or removed, the user event updates the preferences in the Services,
  This triggers the observer which compares the preferences in the local state and the
  preferences in the Services and adds or removes the language in the local state and updates the
    UI to reflect the updated Preferences in the Services.
    */
  removeLanguage(event);
}

/**
 * Event Handler to delete a language selected by the user from the list of
 * Never translate settings list.
 */
function deleteNeverLanguage(event) {
  /* TODO:
  The function removeLanguage removes the HTML element.
  It will be moved to the observer in the next Bug - 1881259 .
  It is here just to test the UI.

  In the next bug we will maintain a local state of preferences.
  When a language is added or removed, the user event updates the preferences in the Services,
  This triggers the observer which compares the preferences in the local state and the
  preferences in the Services and adds or removes the language in the local state and updates the
    UI to reflect the updated Preferences in the Services.
  */
  removeLanguage(event);
}

/**
 * Function to delete a language selected by the user from the list of
 * Always/Never translate settings list.
 */
function removeLanguage(event) {
  /* Langauge section moz-card -parent of-> Language card -parent of->
  Language heading and Language list -parent of->
  Language Element -parent of-> language button and label
  */
  let languageCard = event.target.parentNode.parentNode.parentNode;
  event.target.parentNode.remove();
  if (languageCard.children[1].childElementCount === 0) {
    // If there is no language in the list remove the
    // Language Header and language list div
    languageCard.remove();
  }
}

/**
 * Event Handler to install a language model selected by the user
 */
function installLanguage(event) {
  event.target.classList.remove("translations-settings-download-icon");
  event.target.classList.add("translations-settings-delete-icon");
  event.target.removeEventListener("click", installLanguage);
  event.target.addEventListener("click", unInstallLanguage);
}

/**
 * Event Handler to install a language model selected by the user
 */
function unInstallLanguage(event) {
  event.target.classList.remove("translations-settings-delete-icon");
  event.target.classList.add("translations-settings-download-icon");
  event.target.removeEventListener("click", unInstallLanguage);
  event.target.addEventListener("click", installLanguage);
}
