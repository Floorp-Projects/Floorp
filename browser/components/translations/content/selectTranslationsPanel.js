/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

/**
 * @typedef {import("../../../../toolkit/components/translations/translations").SelectTranslationsPanelState} SelectTranslationsPanelState
 */

ChromeUtils.defineESModuleGetters(this, {
  LanguageDetector:
    "resource://gre/modules/translation/LanguageDetector.sys.mjs",
  TranslationsPanelShared:
    "chrome://browser/content/translations/TranslationsPanelShared.sys.mjs",
  Translator: "chrome://global/content/translations/Translator.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ClipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

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
   * The textarea height for shorter text.
   */
  #shortTextHeight = "8em";

  /**
   * Retrieves the read-only textarea height for shorter text.
   *
   * @see #shortTextHeight
   */
  get shortTextHeight() {
    return this.#shortTextHeight;
  }

  /**
   * The textarea height for shorter text.
   */
  #longTextHeight = "16em";

  /**
   * Retrieves the read-only textarea height for longer text.
   *
   * @see #longTextHeight
   */
  get longTextHeight() {
    return this.#longTextHeight;
  }

  /**
   * The threshold used to determine when the panel should
   * use the short text-height vs. the long-text height.
   */
  #textLengthThreshold = 800;

  /**
   * Retrieves the read-only text-length threshold.
   *
   * @see #textLengthThreshold
   */
  get textLengthThreshold() {
    return this.#textLengthThreshold;
  }

  /**
   * The localized placeholder text to display when idle.
   *
   * @type {string}
   */
  #idlePlaceholderText;

  /**
   * The localized placeholder text to display when translating.
   *
   * @type {string}
   */
  #translatingPlaceholderText;

  /**
   * Where the lazy elements are stored.
   *
   * @type {Record<string, Element>?}
   */
  #lazyElements;

  /**
   * Set to true the first time event listeners are initialized.
   *
   * @type {boolean}
   */
  #eventListenersInitialized = false;

  /**
   * The internal state of the SelectTranslationsPanel.
   *
   * @type {SelectTranslationsPanelState}
   */
  #translationState = { phase: "closed" };

  /**
   * An Id that increments with each translation, used to help keep track
   * of whether an active translation request continue its progression or
   * stop due to the existence of a newer translation request.
   *
   * @type {number}
   */
  #translationId = 0;

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
        cancelButton: "select-translations-panel-cancel-button",
        copyButton: "select-translations-panel-copy-button",
        doneButton: "select-translations-panel-done-button",
        fromLabel: "select-translations-panel-from-label",
        fromMenuList: "select-translations-panel-from",
        fromMenuPopup: "select-translations-panel-from-menupopup",
        header: "select-translations-panel-header",
        initFailureContent: "select-translations-panel-init-failure-content",
        initFailureMessageBar:
          "select-translations-panel-init-failure-message-bar",
        mainContent: "select-translations-panel-main-content",
        settingsButton: "select-translations-panel-settings-button",
        textArea: "select-translations-panel-text-area",
        toLabel: "select-translations-panel-to-label",
        toMenuList: "select-translations-panel-to",
        toMenuPopup: "select-translations-panel-to-menupopup",
        translateButton: "select-translations-panel-translate-button",
        translateFullPageButton:
          "select-translations-panel-translate-full-page-button",
        translationFailureMessageBar:
          "select-translations-panel-translation-failure-message-bar",
        tryAgainButton: "select-translations-panel-try-again-button",
        tryAnotherSourceMenuList:
          "select-translations-panel-try-another-language",
        tryAnotherSourceMenuPopup:
          "select-translations-panel-try-another-language-menupopup",
        unsupportedLanguageContent:
          "select-translations-panel-unsupported-language-content",
        unsupportedLanguageMessageBar:
          "select-translations-panel-unsupported-language-message-bar",
      });
    }

    return this.#lazyElements;
  }

  /**
   * Attempts to determine the best language tag to use as the source language for translation.
   * If the detected language is not supported, attempts to fallback to the document's language tag.
   *
   * @param {string} textToTranslate - The text for which the language detection and target language retrieval are performed.
   *
   * @returns {Promise<string>} - The code of a supported language, a supported document language, or the top detected language.
   */
  async getTopSupportedDetectedLanguage(textToTranslate) {
    // First see if any of the detected languages are supported and return it if so.
    const { language, languages } = await LanguageDetector.detectLanguage(
      textToTranslate
    );
    for (const { languageCode } of languages) {
      const isSupported = await TranslationsParent.isSupportedAsFromLang(
        languageCode
      );
      if (isSupported) {
        return languageCode;
      }
    }

    // Since none of the detected languages were supported, check to see if the
    // document has a specified language tag that is supported.
    const actor = TranslationsParent.getTranslationsActor(
      gBrowser.selectedBrowser
    );
    const detectedLanguages = actor.languageState.detectedLanguages;
    if (detectedLanguages?.isDocLangTagSupported) {
      return detectedLanguages.docLangTag;
    }

    // No supported language was found, so return the top detected language
    // to inform the panel's unsupported language state.
    return language;
  }

  /**
   * Detects the language of the provided text and retrieves a language pair for translation
   * based on user settings.
   *
   * @param {string} textToTranslate - The text for which the language detection and target language retrieval are performed.
   * @returns {Promise<{fromLang?: string, toLang?: string}>} - An object containing the language pair for the translation.
   *   The `fromLang` property is omitted if it is a language that is not currently supported by Firefox Translations.
   */
  async getLangPairPromise(textToTranslate) {
    const [fromLang, toLang] = await Promise.all([
      SelectTranslationsPanel.getTopSupportedDetectedLanguage(textToTranslate),
      TranslationsParent.getTopPreferredSupportedToLang(),
    ]);

    return { fromLang, toLang };
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
    await TranslationsPanelShared.ensureLangListsBuilt(
      document,
      this.elements.panel,
      gBrowser.selectedBrowser.innerWindowID
    );
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
    const { fromMenuList, toMenuList, tryAnotherSourceMenuList } =
      this.elements;
    await Promise.all([
      this.#initializeLanguageMenuList(fromLang, fromMenuList),
      this.#initializeLanguageMenuList(toLang, toMenuList),
      this.#initializeLanguageMenuList(null, tryAnotherSourceMenuList),
    ]);
  }

  /**
   * Initializes event listeners on the panel class the first time
   * this function is called, and is a no-op on subsequent calls.
   */
  #initializeEventListeners() {
    if (this.#eventListenersInitialized) {
      // Event listeners have already been initialized, do nothing.
      return;
    }

    const { panel, fromMenuList, toMenuList, tryAnotherSourceMenuList } =
      this.elements;

    panel.addEventListener("popupshown", this);
    panel.addEventListener("popuphidden", this);

    panel.addEventListener("command", this);
    fromMenuList.addEventListener("command", this);
    toMenuList.addEventListener("command", this);
    tryAnotherSourceMenuList.addEventListener("command", this);

    this.#eventListenersInitialized = true;
  }

  /**
   * Opens the panel, ensuring the panel's UI and state are initialized correctly.
   *
   * @param {Event} event - The triggering event for opening the panel.
   * @param {number} screenX - The x-axis location of the screen at which to open the popup.
   * @param {number} screenY - The y-axis location of the screen at which to open the popup.
   * @param {string} sourceText - The text to translate.
   * @param {Promise} langPairPromise - Promise resolving to language pair data for initializing dropdowns.
   *
   * @returns {Promise<void>}
   */
  async open(event, screenX, screenY, sourceText, langPairPromise) {
    if (this.#isOpen()) {
      return;
    }

    try {
      this.#initializeEventListeners();
      await this.#ensureLangListsBuilt();
      await Promise.all([
        this.#cachePlaceholderText(),
        this.#initializeLanguageMenuLists(langPairPromise),
        this.#registerSourceText(sourceText, langPairPromise),
      ]);
      this.#maybeRequestTranslation();
    } catch (error) {
      this.console?.error(error);
      this.#changeStateToInitFailure(
        event,
        screenX,
        screenY,
        sourceText,
        langPairPromise
      );
    }

    await this.#openPopup(event, screenX, screenY);
  }

  /**
   * Opens a the panel popup at a location on the screen.
   *
   * @param {Event} event - The event that triggers the popup opening.
   * @param {number} screenX - The x-axis location of the screen at which to open the popup.
   * @param {number} screenY - The y-axis location of the screen at which to open the popup.
   */
  async #openPopup(event, screenX, screenY) {
    await window.ensureCustomElements("moz-button-group");
    await window.ensureCustomElements("moz-message-bar");

    this.console?.log("Showing SelectTranslationsPanel");
    const { panel } = this.elements;
    panel.openPopupAtScreen(screenX, screenY, /* isContextMenu */ false, event);
  }

  /**
   * Adds the source text to the translation state and adapts the size of the text area based
   * on the length of the text.
   *
   * @param {string} sourceText - The text to translate.
   * @param {Promise<{fromLang?: string, toLang?: string}>} langPairPromise
   *
   * @returns {Promise<void>}
   */
  async #registerSourceText(sourceText, langPairPromise) {
    const { textArea } = this.elements;
    const { fromLang, toLang } = await langPairPromise;
    const isFromLangSupported = await TranslationsParent.isSupportedAsFromLang(
      fromLang
    );

    if (isFromLangSupported) {
      this.#changeStateTo("idle", /* retainEntries */ false, {
        sourceText,
        fromLanguage: fromLang,
        toLanguage: toLang,
      });
    } else {
      this.#changeStateTo("unsupported", /* retainEntries */ false, {
        sourceText,
        detectedLanguage: fromLang,
        toLanguage: toLang,
      });
    }

    if (sourceText.length < SelectTranslationsPanel.textLengthThreshold) {
      textArea.style.height = SelectTranslationsPanel.shortTextHeight;
    } else {
      textArea.style.height = SelectTranslationsPanel.longTextHeight;
    }
  }

  /**
   * Caches the localized text to use as placeholders.
   */
  async #cachePlaceholderText() {
    const [idleText, translatingText] = await document.l10n.formatValues([
      { id: "select-translations-panel-idle-placeholder-text" },
      { id: "select-translations-panel-translating-placeholder-text" },
    ]);
    this.#idlePlaceholderText = idleText;
    this.#translatingPlaceholderText = translatingText;
  }

  /**
   * Opens the settings menu popup at the settings button gear-icon.
   */
  #openSettingsPopup() {
    const { settingsButton } = this.elements;
    const popup = settingsButton.ownerDocument.getElementById(
      "select-translations-panel-settings-menupopup"
    );
    popup.openPopup(settingsButton, "after_start");
  }

  /**
   * Opens the "About translation in Firefox" Mozilla support page in a new tab.
   */
  onAboutTranslations() {
    this.close();
    const window =
      gBrowser.selectedBrowser.browsingContext.top.embedderElement.ownerGlobal;
    window.openTrustedLinkIn(
      "https://support.mozilla.org/kb/website-translation",
      "tab",
      {
        forceForeground: true,
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      }
    );
  }

  /**
   * Opens the Translations section of about:preferences in a new tab.
   */
  openTranslationsSettingsPage() {
    this.close();
    const window =
      gBrowser.selectedBrowser.browsingContext.top.embedderElement.ownerGlobal;
    window.openTrustedLinkIn("about:preferences#general-translations", "tab");
  }

  /**
   * Handles events when a command event is triggered within the panel.
   *
   * @param {Element} target - The event target
   */
  #handleCommandEvent(target) {
    const {
      cancelButton,
      copyButton,
      doneButton,
      fromMenuList,
      fromMenuPopup,
      settingsButton,
      toMenuList,
      toMenuPopup,
      translateButton,
      translateFullPageButton,
      tryAgainButton,
      tryAnotherSourceMenuList,
      tryAnotherSourceMenuPopup,
    } = this.elements;
    switch (target.id) {
      case cancelButton.id:
      case doneButton.id: {
        this.close();
        break;
      }
      case copyButton.id: {
        this.onClickCopyButton();
        break;
      }
      case fromMenuList.id:
      case fromMenuPopup.id: {
        this.onChangeFromLanguage();
        break;
      }
      case settingsButton.id: {
        this.#openSettingsPopup();
        break;
      }
      case toMenuList.id:
      case toMenuPopup.id: {
        this.onChangeToLanguage();
        break;
      }
      case translateButton.id: {
        this.onClickTranslateButton();
        break;
      }
      case translateFullPageButton.id: {
        this.onClickTranslateFullPageButton();
        break;
      }
      case tryAgainButton.id: {
        this.onClickTryAgainButton();
        break;
      }
      case tryAnotherSourceMenuList.id:
      case tryAnotherSourceMenuPopup.id: {
        this.onChangeTryAnotherSourceLanguage();
        break;
      }
    }
  }

  /**
   * Handles events when a popup is shown within the panel, including showing
   * the panel itself.
   *
   * @param {Element} target - The event target
   */
  #handlePopupShownEvent(target) {
    const { panel, fromMenuPopup, toMenuPopup } = this.elements;
    switch (target.id) {
      case panel.id: {
        this.#updatePanelUIFromState();
        break;
      }
      case fromMenuPopup.id: {
        this.#maybeTranslateOnEvents(["popuphidden"], fromMenuPopup);
        break;
      }
      case toMenuPopup.id: {
        this.#maybeTranslateOnEvents(["popuphidden"], toMenuPopup);
        break;
      }
    }
  }

  /**
   * Handles events when a popup is closed within the panel, including closing
   * the panel itself.
   *
   * @param {Element} target - The event target
   */
  #handlePopupHiddenEvent(target) {
    const { panel } = this.elements;
    switch (target.id) {
      case panel.id: {
        this.#changeStateToClosed();
        break;
      }
    }
  }

  /**
   * Handles events in the SelectTranslationsPanel.
   *
   * @param {Event} event - The event to handle.
   */
  handleEvent(event) {
    let target = event.target;

    // If a menuitem within a menulist is the target, those don't have ids,
    // so we want to traverse until we get to a parent element with an id.
    while (!target.id && target.parentElement) {
      target = target.parentElement;
    }

    switch (event.type) {
      case "command": {
        this.#handleCommandEvent(target);
        break;
      }
      case "popupshown": {
        this.#handlePopupShownEvent(target);
        break;
      }
      case "popuphidden": {
        this.#handlePopupHiddenEvent(target);
        break;
      }
    }
  }

  /**
   * Handles events when the panels select from-language is changed.
   */
  onChangeFromLanguage() {
    const { fromMenuList } = this.elements;
    this.#maybeTranslateOnEvents(["blur", "keypress"], fromMenuList);
  }

  /**
   * Handles events when the panels select to-language is changed.
   */
  onChangeToLanguage() {
    const { toMenuList } = this.elements;
    this.#maybeTranslateOnEvents(["blur", "keypress"], toMenuList);
  }

  /**
   * Handles events when the panel's try-another-source language is changed.
   */
  onChangeTryAnotherSourceLanguage() {
    const { tryAnotherSourceMenuList, translateButton } = this.elements;
    if (tryAnotherSourceMenuList.value) {
      translateButton.disabled = false;
    }
  }

  /**
   * Handles events when the panel's copy button is clicked.
   */
  onClickCopyButton() {
    try {
      ClipboardHelper.copyString(this.getTranslatedText());
    } catch (error) {
      this.console?.error(error);
      return;
    }

    this.#checkCopyButton();
  }

  /**
   * Handles events when the panel's translate button is clicked.
   */
  onClickTranslateButton() {
    const { fromMenuList, tryAnotherSourceMenuList } = this.elements;
    fromMenuList.value = tryAnotherSourceMenuList.value;
    this.#maybeRequestTranslation();
  }

  /**
   * Handles events when the panel's translate-full-page button is clicked.
   */
  onClickTranslateFullPageButton() {
    const { panel } = this.elements;
    const { fromLanguage, toLanguage } = this.#getSelectedLanguagePair();
    const actor = TranslationsParent.getTranslationsActor(
      gBrowser.selectedBrowser
    );
    panel.addEventListener(
      "popuphidden",
      () =>
        actor.translate(
          fromLanguage,
          toLanguage,
          false // reportAsAutoTranslate
        ),
      { once: true }
    );
    this.close();
  }

  /**
   * Handles events when the panel's try-again button is clicked.
   */
  onClickTryAgainButton() {
    switch (this.phase()) {
      case "translation-failure": {
        // If the translation failed, we just need to try translating again.
        this.#maybeRequestTranslation();
        break;
      }
      case "init-failure": {
        // If the initialization failed, we need to close the panel and try reopening it
        // which will attempt to initialize everything again after failure.
        const { panel } = this.elements;
        const { event, screenX, screenY, sourceText, langPairPromise } =
          this.#translationState;

        panel.addEventListener(
          "popuphidden",
          () => this.open(event, screenX, screenY, sourceText, langPairPromise),
          { once: true }
        );

        this.close();
        break;
      }
      default: {
        this.console?.error(
          `Unexpected state "${this.phase()}" on try-again button click.`
        );
      }
    }
  }

  /**
   * Changes the copy button's visual icon to checked, and its localized text to "Copied".
   */
  #checkCopyButton() {
    const { copyButton } = this.elements;
    copyButton.classList.add("copied");
    document.l10n.setAttributes(
      copyButton,
      "select-translations-panel-copy-button-copied"
    );
  }

  /**
   * Changes the copy button's visual icon to unchecked, and its localized text to "Copy".
   */
  #uncheckCopyButton() {
    const { copyButton } = this.elements;
    copyButton.classList.remove("copied");
    document.l10n.setAttributes(
      copyButton,
      "select-translations-panel-copy-button"
    );
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

  /**
   * Focuses on the given menu list if provided and empty, or defaults to focusing one
   * of the from-menu or to-menu lists if either is empty.
   *
   * @param {Element} [menuList] - The menu list to focus if specified.
   */
  #maybeFocusMenuList(menuList) {
    if (menuList && !menuList.value) {
      menuList.focus({ focusVisible: true });
      return;
    }

    const { fromMenuList, toMenuList } = this.elements;
    if (!fromMenuList.value) {
      fromMenuList.focus({ focusVisible: true });
    } else if (!toMenuList.value) {
      toMenuList.focus({ focusVisible: true });
    }
  }

  /**
   * Focuses the translated-text area and sets its overflow to auto post-animation.
   */
  #indicateTranslatedTextArea({ overflow }) {
    const { textArea } = this.elements;
    textArea.focus({ focusVisible: true });
    requestAnimationFrame(() => {
      // We want to set overflow to auto as the final animation, because if it is
      // set before the translated text is displayed, then the scrollTop will
      // move to the bottom as the text is populated.
      //
      // Setting scrollTop = 0 on its own works, but it sometimes causes an animation
      // of the text jumping from the bottom to the top. It looks a lot cleaner to
      // disable overflow before rendering the text, then re-enable it after it renders.
      requestAnimationFrame(() => {
        textArea.style.overflow = overflow;
        textArea.scrollTop = 0;
      });
    });
  }

  /**
   * Checks if the given language pair matches the panel's currently selected language pair.
   *
   * @param {string} fromLanguage - The from-language to compare.
   * @param {string} toLanguage - The to-language to compare.
   *
   * @returns {boolean} - True if the given language pair matches the selected languages in the panel UI, otherwise false.
   */
  #isSelectedLangPair(fromLanguage, toLanguage) {
    const { fromLanguage: selectedFromLang, toLanguage: selectedToLang } =
      this.#getSelectedLanguagePair();
    return fromLanguage === selectedFromLang && toLanguage === selectedToLang;
  }

  /**
   * Retrieves the currently selected language pair from the menu lists.
   *
   * @returns {{fromLanguage: string, toLanguage: string}} An object containing the selected languages.
   */
  #getSelectedLanguagePair() {
    const { fromMenuList, toMenuList } = this.elements;
    return {
      fromLanguage: fromMenuList.value,
      toLanguage: toMenuList.value,
    };
  }

  /**
   * Retrieves the source text from the translation state.
   * This value is not available when the panel is closed.
   *
   * @returns {string | undefined} The source text.
   */
  getSourceText() {
    return this.#translationState?.sourceText;
  }

  /**
   * Retrieves the source text from the translation state.
   * This value is only available in the translated phase.
   *
   * @returns {string | undefined} The translated text.
   */
  getTranslatedText() {
    return this.#translationState?.translatedText;
  }

  /**
   * Retrieves the current phase of the translation state.
   *
   * @returns {string}
   */
  phase() {
    return this.#translationState.phase;
  }

  /**
   * @returns {boolean} True if the panel is open, otherwise false.
   */
  #isOpen() {
    return this.phase() !== "closed";
  }

  /**
   * @returns {boolean} True if the panel is closed, otherwise false.
   */
  #isClosed() {
    return this.phase() === "closed";
  }

  /**
   * Changes the translation state to a new phase with options to retain or overwrite existing entries.
   *
   * @param {SelectTranslationsPanelState} phase - The new phase to transition to.
   * @param {boolean} [retainEntries] - Whether to retain existing state entries that are not overwritten.
   * @param {object | null} [data=null] - Additional data to merge into the state.
   * @throws {Error} If an invalid phase is specified.
   */
  #changeStateTo(phase, retainEntries, data = null) {
    switch (phase) {
      case "closed":
      case "idle":
      case "init-failure":
      case "translation-failure":
      case "translatable":
      case "translating":
      case "translated":
      case "unsupported": {
        // Phase is valid, continue on.
        break;
      }
      default: {
        throw new Error(`Invalid state change to '${phase}'`);
      }
    }

    const previousPhase = this.phase();
    if (data && retainEntries) {
      // Change the phase and apply new entries from data, but retain non-overwritten entries from previous state.
      this.#translationState = { ...this.#translationState, phase, ...data };
    } else if (data) {
      // Change the phase and apply new entries from data, but drop any entries that are not overwritten by data.
      this.#translationState = { phase, ...data };
    } else if (retainEntries) {
      // Change only the phase and retain all entries from previous data.
      this.#translationState.phase = phase;
    } else {
      // Change the phase and delete all entries from previous data.
      this.#translationState = { phase };
    }

    if (previousPhase === this.phase()) {
      // Do not continue on to update the UI because the phase didn't change.
      return;
    }

    const { fromLanguage, toLanguage, detectedLanguage } =
      this.#translationState;
    const sourceLanguage = fromLanguage ? fromLanguage : detectedLanguage;
    this.console?.debug(
      `SelectTranslationsPanel (${sourceLanguage ? sourceLanguage : "??"}-${
        toLanguage ? toLanguage : "??"
      }) state change (${previousPhase} => ${phase})`
    );

    this.#updatePanelUIFromState();
    document.dispatchEvent(
      new CustomEvent("SelectTranslationsPanelStateChanged", {
        detail: { phase },
      })
    );
  }

  /**
   * Changes the phase to closed, discarding any entries in the translation state.
   */
  #changeStateToClosed() {
    this.#changeStateTo("closed", /* retainEntries */ false);
  }

  /**
   * Changes the phase from "translatable" to "translating".
   *
   * @throws {Error} If the current state is not "translatable".
   */
  #changeStateToTranslating() {
    const phase = this.phase();
    if (phase !== "translatable") {
      throw new Error(`Invalid state change (${phase} => translating)`);
    }
    this.#changeStateTo("translating", /* retainEntries */ true);
  }

  /**
   * Changes the phase from "translating" to "translated".
   *
   * @throws {Error} If the current state is not "translating".
   */
  #changeStateToTranslated(translatedText) {
    const phase = this.phase();
    if (phase !== "translating") {
      throw new Error(`Invalid state change (${phase} => translated)`);
    }
    this.#changeStateTo("translated", /* retainEntries */ true, {
      translatedText,
    });
  }

  /**
   * Changes the phase to "init-failure".
   */
  #changeStateToInitFailure(
    event,
    screenX,
    screenY,
    sourceText,
    langPairPromise
  ) {
    this.#changeStateTo("init-failure", /* retainEntries */ true, {
      event,
      screenX,
      screenY,
      sourceText,
      langPairPromise,
    });
  }

  /**
   * Changes the phase from "translating" to "translation-failure".
   */
  #changeStateToTranslationFailure() {
    const phase = this.phase();
    if (phase !== "translating") {
      this.console?.error(
        `Invalid state change (${phase} => translation-failure)`
      );
    }
    this.#changeStateTo("translation-failure", /* retainEntries */ true);
  }

  /**
   * Transitions the phase to "translatable" if the proper conditions are met,
   * otherwise retains the same phase as before.
   *
   * @param {string} fromLanguage - The BCP-47 from-language tag.
   * @param {string} toLanguage - The BCP-47 to-language tag.
   */
  #maybeChangeStateToTranslatable(fromLanguage, toLanguage) {
    const {
      fromLanguage: previousFromLanguage,
      toLanguage: previousToLanguage,
    } = this.#translationState;

    const langSelectionChanged = () =>
      previousFromLanguage !== fromLanguage ||
      previousToLanguage !== toLanguage;

    const shouldTranslateEvenIfLangSelectionHasNotChanged = () => {
      const phase = this.phase();
      return (
        // The panel has just opened, and this is the initial translation.
        phase === "idle" ||
        // The previous translation failed and we are about to try again.
        phase === "translation-failure"
      );
    };

    if (
      // A valid from-language is actively selected.
      fromLanguage &&
      // A valid to-language is actively selected.
      toLanguage &&
      // The language selection has changed, requiring a new translation.
      (langSelectionChanged() ||
        // We should try to translate even if the language selection has not changed.
        shouldTranslateEvenIfLangSelectionHasNotChanged())
    ) {
      this.#changeStateTo("translatable", /* retainEntries */ true, {
        fromLanguage,
        toLanguage,
      });
    }
  }

  /**
   * Handles changes to the copy button based on the current translation state.
   *
   * @param {string} phase - The current phase of the translation state.
   */
  #handleCopyButtonChanges(phase) {
    switch (phase) {
      case "closed":
      case "translation-failure":
      case "translated": {
        this.#uncheckCopyButton();
        break;
      }
      case "idle":
      case "init-failure":
      case "translatable":
      case "translating":
      case "unsupported": {
        // Do nothing.
        break;
      }
      default: {
        throw new Error(`Invalid state change to '${phase}'`);
      }
    }
  }

  /**
   * Handles changes to the text area's background image based on the current translation state.
   *
   * @param {string} phase - The current phase of the translation state.
   */
  #handleTextAreaBackgroundChanges(phase) {
    const { textArea } = this.elements;
    switch (phase) {
      case "translating": {
        textArea.classList.add("translating");
        break;
      }
      case "closed":
      case "idle":
      case "init-failure":
      case "translation-failure":
      case "translatable":
      case "translated":
      case "unsupported": {
        textArea.classList.remove("translating");
        break;
      }
      default: {
        throw new Error(`Invalid state change to '${phase}'`);
      }
    }
  }

  /**
   * Handles changes to the primary UI components based on the current translation state.
   *
   * @param {string} phase  - The current phase of the translation state.
   */
  #handlePrimaryUIChanges(phase) {
    switch (phase) {
      case "closed":
      case "idle": {
        this.#displayIdlePlaceholder();
        break;
      }
      case "init-failure": {
        this.#displayInitFailureMessage();
        break;
      }
      case "translation-failure": {
        this.#displayTranslationFailureMessage();
        break;
      }
      case "translatable": {
        // Do nothing.
        break;
      }
      case "translating": {
        this.#displayTranslatingPlaceholder();
        break;
      }
      case "translated": {
        this.#displayTranslatedText();
        break;
      }
      case "unsupported": {
        this.#displayUnsupportedLanguageMessage();
        break;
      }
      default: {
        throw new Error(`Invalid state change to '${phase}'`);
      }
    }
  }

  /**
   * Determines whether translation should continue based on panel state and language pair.
   *
   * @param {number} translationId - The id of the translation request to match.
   * @param {string} fromLanguage - The from-language to analyze.
   * @param {string} toLanguage - The to-language to analyze.
   *
   * @returns {boolean} True if translation should continue with the given pair, otherwise false.
   */
  #shouldContinueTranslation(translationId, fromLanguage, toLanguage) {
    return (
      // Continue only if the panel is still open.
      this.#isOpen() &&
      // Continue only if the current translationId matches.
      translationId === this.#translationId &&
      // Continue only if the given language pair is still the actively selected pair.
      this.#isSelectedLangPair(fromLanguage, toLanguage)
    );
  }

  /**
   * Displays the placeholder text for the translation state's "idle" phase.
   */
  #displayIdlePlaceholder() {
    this.#showMainContent();

    const { textArea } = SelectTranslationsPanel.elements;
    textArea.value = this.#idlePlaceholderText;
    this.#updateTextDirection();
    this.#updateConditionalUIEnabledState();
    this.#maybeFocusMenuList();
  }

  /**
   * Displays the placeholder text for the translation state's "translating" phase.
   */
  #displayTranslatingPlaceholder() {
    this.#showMainContent();

    const { textArea } = SelectTranslationsPanel.elements;
    textArea.value = this.#translatingPlaceholderText;
    this.#updateTextDirection();
    this.#updateConditionalUIEnabledState();
    this.#indicateTranslatedTextArea({ overflow: "hidden" });
  }

  /**
   * Displays the translated text for the translation state's "translated" phase.
   */
  #displayTranslatedText() {
    this.#showMainContent();

    const { toLanguage } = this.#getSelectedLanguagePair();
    const { textArea } = SelectTranslationsPanel.elements;
    textArea.value = this.getTranslatedText();
    this.#updateTextDirection(toLanguage);
    this.#updateConditionalUIEnabledState();
    this.#indicateTranslatedTextArea({ overflow: "auto" });
  }

  /**
   * Sets attributes on panel elements that are specifically relevant
   * to the SelectTranslationsPanel's state.
   *
   * @param {object} options - Options of which attributes to set.
   * @param {Record<string, Element[]>} options.makeHidden - Make these elements hidden.
   * @param {Record<string, Element[]>} options.makeVisible - Make these elements visible.
   * @param {Record<string, Element[]>} options.addDefault - Give these elements the default attribute.
   * @param {Record<string, Element[]>} options.removeDefault - Remove the default attribute from these elements.
   */
  #setPanelElementAttributes({
    makeHidden = [],
    makeVisible = [],
    addDefault = [],
    removeDefault = [],
  }) {
    for (const element of makeHidden) {
      element.hidden = true;
    }
    for (const element of makeVisible) {
      element.hidden = false;
    }
    for (const element of addDefault) {
      element.setAttribute("default", "true");
    }
    for (const element of removeDefault) {
      element.removeAttribute("default");
    }
  }

  /**
   * Enables or disables UI components that are conditional on a valid language pair being selected.
   */
  #updateConditionalUIEnabledState() {
    const { fromLanguage, toLanguage } = this.#getSelectedLanguagePair();
    const {
      copyButton,
      translateFullPageButton,
      translateButton,
      textArea,
      tryAnotherSourceMenuList,
    } = this.elements;

    const invalidLangPairSelected = !fromLanguage || !toLanguage;
    const isTranslating = this.phase() === "translating";

    textArea.disabled = invalidLangPairSelected;
    translateFullPageButton.disabled = invalidLangPairSelected;
    copyButton.disabled = invalidLangPairSelected || isTranslating;
    translateButton.disabled = !tryAnotherSourceMenuList.value;
  }

  /**
   * Updates the panel UI based on the current phase of the translation state.
   */
  #updatePanelUIFromState() {
    const phase = this.phase();
    this.#handlePrimaryUIChanges(phase);
    this.#handleCopyButtonChanges(phase);
    this.#handleTextAreaBackgroundChanges(phase);
  }

  /**
   * Shows the panel's main-content group of elements.
   */
  #showMainContent() {
    const {
      cancelButton,
      copyButton,
      doneButton,
      initFailureContent,
      mainContent,
      unsupportedLanguageContent,
      textArea,
      translateButton,
      translateFullPageButton,
      translationFailureMessageBar,
      tryAgainButton,
    } = this.elements;
    this.#setPanelElementAttributes({
      makeHidden: [
        cancelButton,
        initFailureContent,
        translateButton,
        translationFailureMessageBar,
        tryAgainButton,
        unsupportedLanguageContent,
      ],
      makeVisible: [
        mainContent,
        copyButton,
        doneButton,
        textArea,
        translateFullPageButton,
      ],
      addDefault: [doneButton],
      removeDefault: [translateButton, tryAgainButton],
    });
  }

  /**
   * Shows the panel's unsupported-language group of elements.
   */
  #showUnsupportedLanguageContent() {
    const {
      cancelButton,
      copyButton,
      doneButton,
      initFailureContent,
      mainContent,
      unsupportedLanguageContent,
      translateButton,
      translateFullPageButton,
      tryAgainButton,
    } = this.elements;
    this.#setPanelElementAttributes({
      makeHidden: [
        cancelButton,
        copyButton,
        initFailureContent,
        mainContent,
        translateFullPageButton,
        tryAgainButton,
      ],
      makeVisible: [doneButton, translateButton, unsupportedLanguageContent],
      addDefault: [translateButton],
      removeDefault: [doneButton, tryAgainButton],
    });
  }

  /**
   * Displays the panel content for when the language dropdowns fail to populate.
   */
  #displayInitFailureMessage() {
    const {
      cancelButton,
      copyButton,
      doneButton,
      initFailureContent,
      mainContent,
      unsupportedLanguageContent,
      translateButton,
      translateFullPageButton,
      tryAgainButton,
    } = this.elements;
    this.#setPanelElementAttributes({
      makeHidden: [
        doneButton,
        copyButton,
        mainContent,
        translateButton,
        translateFullPageButton,
        unsupportedLanguageContent,
      ],
      makeVisible: [initFailureContent, cancelButton, tryAgainButton],
      addDefault: [tryAgainButton],
      removeDefault: [doneButton, translateButton],
    });
  }

  /**
   * Displays the panel content for when a translation fails to complete.
   */
  #displayTranslationFailureMessage() {
    const {
      cancelButton,
      copyButton,
      doneButton,
      initFailureContent,
      mainContent,
      unsupportedLanguageContent,
      textArea,
      translateButton,
      translateFullPageButton,
      translationFailureMessageBar,
      tryAgainButton,
    } = this.elements;
    this.#setPanelElementAttributes({
      makeHidden: [
        doneButton,
        copyButton,
        initFailureContent,
        translateButton,
        translateFullPageButton,
        textArea,
        unsupportedLanguageContent,
      ],
      makeVisible: [
        cancelButton,
        mainContent,
        translationFailureMessageBar,
        tryAgainButton,
      ],
      addDefault: [tryAgainButton],
      removeDefault: [doneButton, translateButton],
    });
  }

  /**
   * Displays the panel's unsupported language message bar, showing
   * the panel's unsupported-language elements.
   */
  #displayUnsupportedLanguageMessage() {
    const { detectedLanguage } = this.#translationState;
    const { unsupportedLanguageMessageBar, tryAnotherSourceMenuList } =
      this.elements;
    const displayNames = new Services.intl.DisplayNames(undefined, {
      type: "language",
    });
    try {
      const language = displayNames.of(detectedLanguage);
      if (language) {
        document.l10n.setAttributes(
          unsupportedLanguageMessageBar,
          "select-translations-panel-unsupported-language-message-known",
          { language }
        );
      } else {
        // Will be immediately caught.
        throw new Error();
      }
    } catch {
      // Either displayNames.of() threw, or we threw due to no display name found.
      // In either case, localize the message for an unknown language.
      document.l10n.setAttributes(
        unsupportedLanguageMessageBar,
        "select-translations-panel-unsupported-language-message-unknown"
      );
    }
    this.#updateConditionalUIEnabledState();
    this.#showUnsupportedLanguageContent();
    this.#maybeFocusMenuList(tryAnotherSourceMenuList);
  }

  /**
   * Sets the text direction attribute in the text areas based on the specified language.
   * Uses the given language tag if provided, otherwise uses the current app locale.
   *
   * @param {string} [langTag] - The language tag to determine text direction.
   */
  #updateTextDirection(langTag) {
    const { textArea } = this.elements;
    if (langTag) {
      const scriptDirection = Services.intl.getScriptDirection(langTag);
      textArea.setAttribute("dir", scriptDirection);
    } else {
      textArea.removeAttribute("dir");
    }
  }

  /**
   * Requests a translations port for a given language pair.
   *
   * @param {string} fromLanguage - The from-language.
   * @param {string} toLanguage - The to-language.
   *
   * @returns {Promise<MessagePort | undefined>} The message port promise.
   */
  async #requestTranslationsPort(fromLanguage, toLanguage) {
    const innerWindowId =
      gBrowser.selectedBrowser.browsingContext.top.embedderElement
        .innerWindowID;
    if (!innerWindowId) {
      return undefined;
    }
    const port = await TranslationsParent.requestTranslationsPort(
      innerWindowId,
      fromLanguage,
      toLanguage
    );
    return port;
  }

  /**
   * Retrieves the existing translator for the specified language pair if it matches,
   * otherwise creates a new translator.
   *
   * @param {string} fromLanguage - The source language code.
   * @param {string} toLanguage - The target language code.
   *
   * @returns {Promise<Translator>} A promise that resolves to a `Translator` instance for the given language pair.
   */
  async #createTranslator(fromLanguage, toLanguage) {
    this.console?.log(
      `Creating new Translator (${fromLanguage}-${toLanguage})`
    );

    const translator = await Translator.create(fromLanguage, toLanguage, {
      allowSameLanguage: true,
      requestTranslationsPort: this.#requestTranslationsPort,
    });
    return translator;
  }

  /**
   * Initiates the translation process if the panel state and selected languages
   * meet the conditions for translation.
   */
  #maybeRequestTranslation() {
    if (this.#isClosed()) {
      return;
    }

    const { fromLanguage, toLanguage } = this.#getSelectedLanguagePair();
    this.#maybeChangeStateToTranslatable(fromLanguage, toLanguage);

    if (this.phase() !== "translatable") {
      return;
    }

    const translationId = ++this.#translationId;
    this.#createTranslator(fromLanguage, toLanguage)
      .then(translator => {
        if (
          this.#shouldContinueTranslation(
            translationId,
            fromLanguage,
            toLanguage
          )
        ) {
          this.#changeStateToTranslating();
          return translator.translate(this.getSourceText());
        }
        return null;
      })
      .then(translatedText => {
        if (
          translatedText &&
          this.#shouldContinueTranslation(
            translationId,
            fromLanguage,
            toLanguage
          )
        ) {
          this.#changeStateToTranslated(translatedText);
        }
      })
      .catch(error => {
        this.console?.error(error);
        this.#changeStateToTranslationFailure();
      });
  }

  /**
   * Attaches event listeners to the target element for initiating translation on specified event types.
   *
   * @param {string[]} eventTypes - An array of event types to listen for.
   * @param {object} target - The target element to attach event listeners to.
   * @throws {Error} If an unrecognized event type is provided.
   */
  #maybeTranslateOnEvents(eventTypes, target) {
    if (!target.translationListenerCallbacks) {
      target.translationListenerCallbacks = [];
    }
    if (target.translationListenerCallbacks.length === 0) {
      for (const eventType of eventTypes) {
        let callback;
        switch (eventType) {
          case "blur":
          case "popuphidden": {
            callback = () => {
              this.#maybeRequestTranslation();
              this.#removeTranslationListeners(target);
            };
            break;
          }
          case "keypress": {
            callback = event => {
              if (event.key === "Enter") {
                this.#maybeRequestTranslation();
              }
              this.#removeTranslationListeners(target);
            };
            break;
          }
          default: {
            throw new Error(
              `Invalid translation event type given: '${eventType}`
            );
          }
        }
        target.addEventListener(eventType, callback, { once: true });
        target.translationListenerCallbacks.push({ eventType, callback });
      }
    }
  }

  /**
   * Removes all translation event listeners from the target element.
   *
   * @param {Element} target - The element from which event listeners are to be removed.
   */
  #removeTranslationListeners(target) {
    for (const { eventType, callback } of target.translationListenerCallbacks) {
      target.removeEventListener(eventType, callback);
    }
    target.translationListenerCallbacks = [];
  }
})();
