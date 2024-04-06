/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

ChromeUtils.defineESModuleGetters(this, {
  LanguageDetector:
    "resource://gre/modules/translation/LanguageDetector.sys.mjs",
  TranslationsPanelShared:
    "chrome://browser/content/translations/TranslationsPanelShared.sys.mjs",
});

/**
 * This singleton class controls the Translations popup panel.
 */
var SelectTranslationsPanel = new (class {
  /** @type {Console?} */
  #console;

  /**
   * Lazily get a console instance. Note that this script is loaded in very early to
   * the browser loading process, and may run before the console is available. In
   * this case the console will return as `undefined`.
   *
   * @returns {Console | void}
   */
  get console() {
    if (!this.#console) {
      try {
        this.#console = console.createInstance({
          maxLogLevelPref: "browser.translations.logLevel",
          prefix: "Translations",
        });
      } catch {
        // The console may not be initialized yet.
      }
    }
    return this.#console;
  }

  /**
   * Where the lazy elements are stored.
   *
   * @type {Record<string, Element>?}
   */
  #lazyElements;

  /**
   * Lazily creates the dom elements, and lazily selects them.
   *
   * @returns {Record<string, Element>}
   */
  get elements() {
    if (!this.#lazyElements) {
      // Lazily turn the template into a DOM element.
      /** @type {HTMLTemplateElement} */
      const wrapper = document.getElementById(
        "template-select-translations-panel"
      );

      const panel = wrapper.content.firstElementChild;
      const settingsButton = document.getElementById(
        "translations-panel-settings"
      );
      wrapper.replaceWith(wrapper.content);

      // Lazily select the elements.
      this.#lazyElements = {
        panel,
        settingsButton,
      };

      TranslationsPanelShared.defineLazyElements(document, this.#lazyElements, {
        betaIcon: "select-translations-panel-beta-icon",
        copyButton: "select-translations-panel-copy-button",
        doneButton: "select-translations-panel-done-button",
        fromLabel: "select-translations-panel-from-label",
        fromMenuList: "select-translations-panel-from",
        header: "select-translations-panel-header",
        multiview: "select-translations-panel-multiview",
        textArea: "select-translations-panel-translation-area",
        toLabel: "select-translations-panel-to-label",
        toMenuList: "select-translations-panel-to",
        translateFullPageButton:
          "select-translations-panel-translate-full-page-button",
      });
    }

    return this.#lazyElements;
  }

  /**
   * Detects the language of the provided text and retrieves a language pair for translation
   * based on user settings.
   *
   * @param {string} textToTranslate - The text for which the language detection and target language retrieval are performed.
   * @returns {Promise<{fromLang?: string, toLang?: string}>} - An object containing the language pair for the translation.
   *   The `fromLang` property is omitted if it is a language that is not currently supported by Firefox Translations.
   *   The `toLang` property is omitted if it is the same as `fromLang`.
   */
  async getLangPairPromise(textToTranslate) {
    const [fromLang, toLang] = await Promise.all([
      LanguageDetector.detectLanguage(textToTranslate).then(
        ({ language }) => language
      ),
      TranslationsParent.getTopPreferredSupportedToLang(),
    ]);

    return {
      fromLang,
      // If the fromLang and toLang are the same, discard the toLang.
      toLang: fromLang === toLang ? undefined : toLang,
    };
  }

  /**
   * Close the Select Translations Panel.
   */
  close() {
    PanelMultiView.hidePopup(this.elements.panel);
  }

  /**
   * Ensures that the from-language and to-language dropdowns are built.
   *
   * This can be called every time the popup is shown, since it will retry
   * when there is an error (such as a network error) or be a no-op if the
   * dropdowns have already been initialized.
   */
  async #ensureLangListsBuilt() {
    try {
      await TranslationsPanelShared.ensureLangListsBuilt(
        document,
        this.elements.panel,
        gBrowser.selectedBrowser.innerWindowID
      );
    } catch (error) {
      this.console?.error(error);
    }
  }

  /**
   * Initializes the selected value of the given language dropdown based on the language tag.
   *
   * @param {string} langTag - A BCP-47 language tag.
   * @param {Element} menuList - The menu list element to update.
   *
   * @returns {Promise<void>}
   */
  async #initializeLanguageMenuList(langTag, menuList) {
    const isLangTagSupported =
      menuList.id === this.elements.fromMenuList.id
        ? await TranslationsParent.isSupportedAsFromLang(langTag)
        : await TranslationsParent.isSupportedAsToLang(langTag);

    if (isLangTagSupported) {
      // Remove the data-l10n-id because the menulist label will
      // be populated from the supported language's display name.
      menuList.removeAttribute("data-l10n-id");
      menuList.value = langTag;
    } else {
      await this.#deselectLanguage(menuList);
    }
  }

  /**
   * Initializes the selected values of the from-language and to-language menu
   * lists based on the result of the given language pair promise.
   *
   * @param {Promise<{fromLang?: string, toLang?: string}>} langPairPromise
   *
   * @returns {Promise<void>}
   */
  async #initializeLanguageMenuLists(langPairPromise) {
    const { fromLang, toLang } = await langPairPromise;
    const { fromMenuList, toMenuList } = this.elements;
    await Promise.all([
      this.#initializeLanguageMenuList(fromLang, fromMenuList),
      this.#initializeLanguageMenuList(toLang, toMenuList),
    ]);
  }

  /**
   * Opens the panel and populates the currently selected fromLang and toLang based
   * on the result of the langPairPromise.
   *
   * @param {Event} event - The triggering event for opening the panel.
   * @param {Promise} langPairPromise - Promise resolving to language pair data for initializing dropdowns.
   * @returns {Promise<void>}
   */
  async open(event, langPairPromise) {
    this.console?.log("Showing a translation panel.");

    await this.#ensureLangListsBuilt();
    await this.#initializeLanguageMenuLists(langPairPromise);

    // TODO(Bug 1878721) Rework the logic of where to open the panel.
    //
    // For the moment, the Select Translations panel opens at the
    // AppMenu Button, but it will eventually need to open near
    // to the selected content.
    const appMenuButton = document.getElementById("PanelUI-menu-button");
    const { panel, textArea } = this.elements;

    panel.addEventListener("popupshown", () => textArea.focus(), {
      once: true,
    });
    await PanelMultiView.openPopup(panel, appMenuButton, {
      position: "bottomright topright",
      triggerEvent: event,
    }).catch(error => this.console?.error(error));
  }

  /**
   * Clears the selected language and ensures that the menu list displays
   * the proper placeholder text.
   *
   * @param {Element} menuList - The target menu list element to update.
   */
  async #deselectLanguage(menuList) {
    menuList.value = "";
    document.l10n.setAttributes(menuList, "translations-panel-choose-language");
    await document.l10n.translateElements([menuList]);
  }
})();
