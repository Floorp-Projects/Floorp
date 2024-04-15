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
   * The internal state of the SelectTranslationsPanel.
   *
   * @type {SelectTranslationsPanelState}
   */
  #translationState = { phase: "closed" };

  /**
   * The Translator for the current language pair.
   *
   * @type {Translator}
   */
  #translator;

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
        copyButton: "select-translations-panel-copy-button",
        doneButton: "select-translations-panel-done-button",
        fromLabel: "select-translations-panel-from-label",
        fromMenuList: "select-translations-panel-from",
        fromMenuPopup: "select-translations-panel-from-menupopup",
        header: "select-translations-panel-header",
        textArea: "select-translations-panel-text-area",
        toLabel: "select-translations-panel-to-label",
        toMenuList: "select-translations-panel-to",
        toMenuPopup: "select-translations-panel-to-menupopup",
        translateFullPageButton:
          "select-translations-panel-translate-full-page-button",
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
   *   The `toLang` property is omitted if it is the same as `fromLang`.
   */
  async getLangPairPromise(textToTranslate) {
    const [fromLang, toLang] = await Promise.all([
      SelectTranslationsPanel.getTopSupportedDetectedLanguage(textToTranslate),
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

    this.#registerSourceText(sourceText);
    await this.#ensureLangListsBuilt();

    await Promise.all([
      this.#cachePlaceholderText(),
      this.#initializeLanguageMenuLists(langPairPromise),
    ]);

    this.#displayIdlePlaceholder();
    this.#maybeRequestTranslation();
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

    this.console?.log("Showing SelectTranslationsPanel");
    const { panel } = this.elements;
    panel.openPopupAtScreen(screenX, screenY, /* isContextMenu */ false, event);
  }

  /**
   * Adds the source text to the translation state and adapts the size of the text area based
   * on the length of the text.
   *
   * @param {string} sourceText - The text to translate.
   *
   * @returns {Promise<void>}
   */
  #registerSourceText(sourceText) {
    const { textArea } = this.elements;
    this.#changeStateTo("idle", /* retainEntries */ false, {
      sourceText,
    });

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
   * Handles events when a popup is shown within the panel, including showing
   * the panel itself.
   *
   * @param {Event} event - The event that triggered the popup to show.
   */
  handlePanelPopupShownEvent(event) {
    const { panel, fromMenuPopup, toMenuPopup } = this.elements;
    switch (event.target.id) {
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
   * @param {Event} event - The event that triggered the popup to close.
   */
  handlePanelPopupHiddenEvent(event) {
    const { panel } = this.elements;
    switch (event.target.id) {
      case panel.id: {
        this.#changeStateToClosed();
        break;
      }
    }
  }

  /**
   * Handles events when the panels select from-language is changed.
   */
  onChangeFromLanguage() {
    const { fromMenuList, toMenuList } = this.elements;
    this.#maybeTranslateOnEvents(["blur", "keypress"], fromMenuList);
    this.#maybeStealLanguageFrom(toMenuList);
  }

  /**
   * Handles events when the panels select to-language is changed.
   */
  onChangeToLanguage() {
    const { toMenuList, fromMenuList } = this.elements;
    this.#maybeTranslateOnEvents(["blur", "keypress"], toMenuList);
    this.#maybeStealLanguageFrom(fromMenuList);
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
   * Deselects the language from the target menu list if both menu lists
   * have the same language selected, simulating the effect of one menu
   * list stealing the selected language value from the other.
   *
   * @param {Element} menuList - The target menu list element to update.
   */
  async #maybeStealLanguageFrom(menuList) {
    const { fromLanguage, toLanguage } = this.#getSelectedLanguagePair();
    if (fromLanguage === toLanguage) {
      await this.#deselectLanguage(menuList);
      this.#maybeFocusMenuList(menuList);
    }
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
   * Checks if the translator's language configuration matches the given language pair.
   *
   * @param {string} fromLanguage - The from-language to compare.
   * @param {string} toLanguage - The to-language to compare.
   *
   * @returns {boolean} - True if the translator's languages match the given pair, otherwise false.
   */
  #translatorMatchesLangPair(fromLanguage, toLanguage) {
    return (
      this.#translator?.fromLanguage === fromLanguage &&
      this.#translator?.toLanguage === toLanguage
    );
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
    const { textArea } = this.elements;
    switch (phase) {
      case "translating": {
        textArea.classList.add("translating");
        break;
      }
      case "closed":
      case "idle":
      case "translatable":
      case "translated": {
        textArea.classList.remove("translating");
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

    const { fromLanguage, toLanguage } = this.#translationState;
    this.console?.debug(
      `SelectTranslationsPanel (${fromLanguage ? fromLanguage : "??"}-${
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
   * Transitions the phase of the state based on the given language pair.
   *
   * @param {string} fromLanguage - The BCP-47 from-language tag.
   * @param {string} toLanguage - The BCP-47 to-language tag.
   *
   * @returns {SelectTranslationsPanelState} The new phase of the translation state.
   */
  #changeStateByLanguagePair(fromLanguage, toLanguage) {
    const {
      phase: previousPhase,
      fromLanguage: previousFromLanguage,
      toLanguage: previousToLanguage,
    } = this.#translationState;

    let nextPhase = "translatable";

    if (
      // No from-language is selected, so we cannot translate.
      !fromLanguage ||
      // No to-language is selected, so we cannot translate.
      !toLanguage ||
      // The same language has been selected, so we cannot translate.
      fromLanguage === toLanguage
    ) {
      nextPhase = "idle";
    } else if (
      // The languages have not changed, so there is nothing to do.
      previousFromLanguage === fromLanguage &&
      previousToLanguage === toLanguage
    ) {
      nextPhase = previousPhase;
    }

    this.#changeStateTo(nextPhase, /* retainEntries */ true, {
      fromLanguage,
      toLanguage,
    });

    return nextPhase;
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
      this.#isSelectedLangPair(fromLanguage, toLanguage) &&
      // Continue only if the given language pair matches the current translator.
      this.#translatorMatchesLangPair(fromLanguage, toLanguage)
    );
  }

  /**
   * Displays the placeholder text for the translation state's "idle" phase.
   */
  #displayIdlePlaceholder() {
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
    const { toLanguage } = this.#getSelectedLanguagePair();
    const { textArea } = SelectTranslationsPanel.elements;
    textArea.value = this.getTranslatedText();
    this.#updateTextDirection(toLanguage);
    this.#updateConditionalUIEnabledState();
    this.#indicateTranslatedTextArea({ overflow: "auto" });
  }

  /**
   * Enables or disables UI components that are conditional on a valid language pair being selected.
   */
  #updateConditionalUIEnabledState() {
    const { fromLanguage, toLanguage } = this.#getSelectedLanguagePair();
    const { copyButton, translateFullPageButton, textArea } = this.elements;

    const invalidLangPairSelected = !fromLanguage || !toLanguage;
    const isTranslating = this.phase() === "translating";

    textArea.disabled = invalidLangPairSelected;
    translateFullPageButton.disabled = invalidLangPairSelected;
    copyButton.disabled = invalidLangPairSelected || isTranslating;
  }

  /**
   * Updates the panel UI based on the current phase of the translation state.
   */
  #updatePanelUIFromState() {
    switch (this.phase()) {
      case "idle": {
        this.#displayIdlePlaceholder();
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
    }
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
  async #getOrCreateTranslator(fromLanguage, toLanguage) {
    if (this.#translatorMatchesLangPair(fromLanguage, toLanguage)) {
      return this.#translator;
    }

    this.console?.log(
      `Creating new Translator (${fromLanguage}-${toLanguage})`
    );
    if (this.#translator) {
      this.#translator.destroy();
      this.#translator = null;
    }

    this.#translator = await Translator.create(fromLanguage, toLanguage, {
      requestTranslationsPort: this.#requestTranslationsPort,
    });
    return this.#translator;
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
    const nextState = this.#changeStateByLanguagePair(fromLanguage, toLanguage);
    if (nextState !== "translatable") {
      return;
    }

    const translationId = ++this.#translationId;
    this.#getOrCreateTranslator(fromLanguage, toLanguage)
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
        } else if (this.#isOpen()) {
          this.#changeStateTo("idle", /* retainEntires */ false, {
            sourceText: this.getSourceText(),
          });
        }
      })
      .catch(error => this.console?.error(error));
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
