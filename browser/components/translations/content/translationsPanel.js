/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

/* eslint-disable jsdoc/valid-types */
/**
 * @typedef {import("../../../../toolkit/components/translations/translations").LangTags} LangTags
 */
/* eslint-enable jsdoc/valid-types */

ChromeUtils.defineESModuleGetters(this, {
  PageActions: "resource:///modules/PageActions.sys.mjs",
  TranslationsTelemetry:
    "chrome://browser/content/translations/TranslationsTelemetry.sys.mjs",
});

/**
 * The set of actions that can occur from interaction with the
 * translations panel.
 */
const PageAction = Object.freeze({
  NO_CHANGE: "NO_CHANGE",
  RESTORE_PAGE: "RESTORE_PAGE",
  TRANSLATE_PAGE: "TRANSLATE_PAGE",
});

/**
 * A mechanism for determining the next relevant page action
 * based on the current translated state of the page and the state
 * of the persistent options in the translations panel settings.
 */
class CheckboxPageAction {
  /**
   * Whether or not translations is active on the page.
   *
   * @type {boolean}
   */
  #translationsActive = false;

  /**
   * Whether the always-translate-language menuitem is checked
   * in the translations panel settings menu.
   *
   * @type {boolean}
   */
  #alwaysTranslateLanguage = false;

  /**
   * Whether the never-translate-language menuitem is checked
   * in the translations panel settings menu.
   *
   * @type {boolean}
   */
  #neverTranslateLanguage = false;

  /**
   * Whether the never-translate-site menuitem is checked
   * in the translations panel settings menu.
   *
   * @type {boolean}
   */
  #neverTranslateSite = false;

  /**
   * @param {boolean} translationsActive
   * @param {boolean} alwaysTranslateLanguage
   * @param {boolean} neverTranslateLanguage
   * @param {boolean} neverTranslateSite
   */
  constructor(
    translationsActive,
    alwaysTranslateLanguage,
    neverTranslateLanguage,
    neverTranslateSite
  ) {
    this.#translationsActive = translationsActive;
    this.#alwaysTranslateLanguage = alwaysTranslateLanguage;
    this.#neverTranslateLanguage = neverTranslateLanguage;
    this.#neverTranslateSite = neverTranslateSite;
  }

  /**
   * Accepts four integers that are either 0 or 1 and returns
   * a single, unique number for each possible combination of
   * values.
   *
   * @param {number} translationsActive
   * @param {number} alwaysTranslateLanguage
   * @param {number} neverTranslateLanguage
   * @param {number} neverTranslateSite
   *
   * @returns {number} - An integer representation of the state
   */
  static #computeState(
    translationsActive,
    alwaysTranslateLanguage,
    neverTranslateLanguage,
    neverTranslateSite
  ) {
    return (
      (translationsActive << 3) |
      (alwaysTranslateLanguage << 2) |
      (neverTranslateLanguage << 1) |
      neverTranslateSite
    );
  }

  /**
   * Returns the current state of the data members as a single number.
   *
   * @returns {number} - An integer representation of the state
   */
  #state() {
    return CheckboxPageAction.#computeState(
      Number(this.#translationsActive),
      Number(this.#alwaysTranslateLanguage),
      Number(this.#neverTranslateLanguage),
      Number(this.#neverTranslateSite)
    );
  }

  /**
   * Returns the next page action to take when the always-translate-language
   * menuitem is toggled in the translations panel settings menu.
   *
   * @returns {PageAction}
   */
  alwaysTranslateLanguage() {
    switch (this.#state()) {
      case CheckboxPageAction.#computeState(1, 1, 0, 1):
      case CheckboxPageAction.#computeState(1, 1, 0, 0):
        return PageAction.RESTORE_PAGE;
      case CheckboxPageAction.#computeState(0, 0, 1, 0):
      case CheckboxPageAction.#computeState(0, 0, 0, 0):
        return PageAction.TRANSLATE_PAGE;
    }
    return PageAction.NO_CHANGE;
  }

  /**
   * Returns the next page action to take when the never-translate-language
   * menuitem is toggled in the translations panel settings menu.
   *
   * @returns {PageAction}
   */
  neverTranslateLanguage() {
    switch (this.#state()) {
      case CheckboxPageAction.#computeState(1, 1, 0, 1):
      case CheckboxPageAction.#computeState(1, 1, 0, 0):
      case CheckboxPageAction.#computeState(1, 0, 0, 1):
      case CheckboxPageAction.#computeState(1, 0, 0, 0):
        return PageAction.RESTORE_PAGE;
    }
    return PageAction.NO_CHANGE;
  }

  /**
   * Returns the next page action to take when the never-translate-site
   * menuitem is toggled in the translations panel settings menu.
   *
   * @returns {PageAction}
   */
  neverTranslateSite() {
    switch (this.#state()) {
      case CheckboxPageAction.#computeState(1, 1, 0, 0):
      case CheckboxPageAction.#computeState(1, 0, 1, 0):
      case CheckboxPageAction.#computeState(1, 0, 0, 0):
        return PageAction.RESTORE_PAGE;
      case CheckboxPageAction.#computeState(0, 1, 0, 1):
        return PageAction.TRANSLATE_PAGE;
    }
    return PageAction.NO_CHANGE;
  }
}

/**
 * This singleton class controls the Translations popup panel.
 *
 * This component is a `/browser` component, and the actor is a `/toolkit` actor, so care
 * must be taken to keep the presentation (this component) from the state management
 * (the Translations actor). This class reacts to state changes coming from the
 * Translations actor.
 */
var TranslationsPanel = new (class {
  /** @type {Console?} */
  #console;

  /**
   * The cached detected languages for both the document and the user.
   *
   * @type {null | LangTags}
   */
  detectedLanguages = null;

  /**
   * Lazily get a console instance. Note that this script is loaded in very early to
   * the browser loading process, and may run before the console is avialable. In
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
   * Tracks if the popup is open, or scheduled to be open.
   *
   * @type {boolean}
   */
  #isPopupOpen = false;

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
      const wrapper = document.getElementById("template-translations-panel");
      const panel = wrapper.content.firstElementChild;
      wrapper.replaceWith(wrapper.content);

      const settingsButton = document.getElementById(
        "translations-panel-settings"
      );
      // Clone the settings toolbarbutton across all the views.
      for (const header of panel.querySelectorAll(".panel-header")) {
        if (header.contains(settingsButton)) {
          continue;
        }
        const settingsButtonClone = settingsButton.cloneNode(true);
        settingsButtonClone.removeAttribute("id");
        header.appendChild(settingsButtonClone);
      }

      // Lazily select the elements.
      this.#lazyElements = {
        panel,
        settingsButton,
        // The rest of the elements are set by the getter below.
      };

      /**
       * Define a getter on #lazyElements that gets the element by an id
       * or class name.
       */
      const getter = (name, discriminator) => {
        let element;
        Object.defineProperty(this.#lazyElements, name, {
          get: () => {
            if (!element) {
              if (discriminator[0] === ".") {
                // Lookup by class
                element = document.querySelector(discriminator);
              } else {
                // Lookup by id
                element = document.getElementById(discriminator);
              }
            }
            if (!element) {
              throw new Error(
                `Could not find "${name}" at "#${discriminator}".`
              );
            }
            return element;
          },
        });
      };

      // Getters by id
      getter("appMenuButton", "PanelUI-menu-button");
      getter("cancelButton", "translations-panel-cancel");
      getter(
        "changeSourceLanguageButton",
        "translations-panel-change-source-language"
      );
      getter("dismissErrorButton", "translations-panel-dismiss-error");
      getter("error", "translations-panel-error");
      getter("errorMessage", "translations-panel-error-message");
      getter("errorMessageHint", "translations-panel-error-message-hint");
      getter("errorHintAction", "translations-panel-translate-hint-action");
      getter("fromMenuList", "translations-panel-from");
      getter("fromLabel", "translations-panel-from-label");
      getter("header", "translations-panel-header");
      getter("intro", "translations-panel-intro");
      getter("introLearnMoreLink", "translations-panel-intro-learn-more-link");
      getter("langSelection", "translations-panel-lang-selection");
      getter("multiview", "translations-panel-multiview");
      getter("restoreButton", "translations-panel-restore-button");
      getter("toLabel", "translations-panel-to-label");
      getter("toMenuList", "translations-panel-to");
      getter("translateButton", "translations-panel-translate");
      getter(
        "unsupportedHeader",
        "translations-panel-unsupported-language-header"
      );
      getter("unsupportedHint", "translations-panel-error-unsupported-hint");
      getter(
        "unsupportedLearnMoreLink",
        "translations-panel-unsupported-learn-more-link"
      );

      // Getters by class
      getter(
        "alwaysTranslateLanguageMenuItem",
        ".always-translate-language-menuitem"
      );
      getter("manageLanguagesMenuItem", ".manage-languages-menuitem");
      getter(
        "neverTranslateLanguageMenuItem",
        ".never-translate-language-menuitem"
      );
      getter("neverTranslateSiteMenuItem", ".never-translate-site-menuitem");
    }

    return this.#lazyElements;
  }

  #lazyButtonElements = null;

  /**
   * When accessing `this.elements` the first time, it de-lazifies the custom components
   * that are needed for the popup. Avoid that by having a second element lookup
   * just for modifying the button.
   */
  get buttonElements() {
    if (!this.#lazyButtonElements) {
      this.#lazyButtonElements = {
        button: document.getElementById("translations-button"),
        buttonLocale: document.getElementById("translations-button-locale"),
        buttonCircleArrows: document.getElementById(
          "translations-button-circle-arrows"
        ),
      };
    }
    return this.#lazyButtonElements;
  }

  /**
   * Cache the last command used for error hints so that it can be later removed.
   */
  #lastHintCommand = null;

  /**
   * @param {object} options
   * @param {string} options.message - l10n id
   * @param {string} options.hint - l10n id
   * @param {string} options.actionText - l10n id
   * @param {Function} options.actionCommand - The action to perform.
   */
  #showError({
    message,
    hint,
    actionText: hintCommandText,
    actionCommand: hintCommand,
  }) {
    const { error, errorMessage, errorMessageHint, errorHintAction, intro } =
      this.elements;
    error.hidden = false;
    intro.hidden = true;
    document.l10n.setAttributes(errorMessage, message);

    if (hint) {
      errorMessageHint.hidden = false;
      document.l10n.setAttributes(errorMessageHint, hint);
    } else {
      errorMessageHint.hidden = true;
    }

    if (hintCommand && hintCommandText) {
      errorHintAction.removeEventListener("command", this.#lastHintCommand);
      this.#lastHintCommand = hintCommand;
      errorHintAction.addEventListener("command", hintCommand);
      errorHintAction.hidden = false;
      document.l10n.setAttributes(errorHintAction, hintCommandText);
    } else {
      errorHintAction.hidden = true;
    }
  }

  /**
   * @returns {TranslationsParent}
   */
  #getTranslationsActor() {
    const actor =
      gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getActor(
        "Translations"
      );

    if (!actor) {
      throw new Error("Unable to get the TranslationsParent");
    }
    return actor;
  }

  /**
   * Fetches the language tags for the document and the user and caches the results
   * Use `#getCachedDetectedLanguages` when the lang tags do not need to be re-fetched.
   * This requires a bit of work to do, so prefer the cached version when possible.
   *
   * @returns {Promise<LangTags>}
   */
  async #fetchDetectedLanguages() {
    this.detectedLanguages =
      await this.#getTranslationsActor().getDetectedLanguages();
    return this.detectedLanguages;
  }

  /**
   * If the detected language tags have been retrieved previously, return the cached
   * version. Otherwise do a fresh lookup of the document's language tag.
   *
   * @returns {Promise<LangTags>}
   */
  async #getCachedDetectedLanguages() {
    if (!this.detectedLanguages) {
      return this.#fetchDetectedLanguages();
    }
    return this.detectedLanguages;
  }

  /**
   * @type {"initialized" | "error" | "uninitialized"}
   */
  #langListsPhase = "uninitialized";

  /**
   * Builds the <menulist> of languages for both the "from" and "to". This can be
   * called every time the popup is shown, as it will retry when there is an error
   * (such as a network error) or be a noop if it's already initialized.
   *
   * TODO(Bug 1813796) This needs to be updated when the supported languages change
   * via RemoteSettings.
   */
  async #ensureLangListsBuilt() {
    switch (this.#langListsPhase) {
      case "initialized":
        // This has already been initialized.
        return;
      case "error":
        // Attempt to re-initialize.
        this.#langListsPhase = "uninitialized";
        break;
      case "uninitialized":
        // Ready to initialize.
        break;
      default:
        this.console?.error("Unknown langList phase", this.#langListsPhase);
    }

    try {
      /** @type {SupportedLanguages} */
      const { languagePairs, fromLanguages, toLanguages } =
        await TranslationsParent.getSupportedLanguages();

      // Verify that we are in a proper state.
      if (languagePairs.length === 0) {
        throw new Error("No translation languages were retrieved.");
      }

      const { panel } = this.elements;
      const fromPopups = panel.querySelectorAll(
        ".translations-panel-language-menupopup-from"
      );
      const toPopups = panel.querySelectorAll(
        ".translations-panel-language-menupopup-to"
      );

      for (const popup of fromPopups) {
        for (const { langTag, displayName } of fromLanguages) {
          const fromMenuItem = document.createXULElement("menuitem");
          fromMenuItem.setAttribute("value", langTag);
          fromMenuItem.setAttribute("label", displayName);
          popup.appendChild(fromMenuItem);
        }
      }

      for (const popup of toPopups) {
        for (const { langTag, displayName } of toLanguages) {
          const toMenuItem = document.createXULElement("menuitem");
          toMenuItem.setAttribute("value", langTag);
          toMenuItem.setAttribute("label", displayName);
          popup.appendChild(toMenuItem);
        }
      }

      this.#langListsPhase = "initialized";
    } catch (error) {
      this.console?.error(error);
      this.#langListsPhase = "error";
    }
  }

  /**
   * Reactively sets the views based on the async state changes of the engine, and
   * other component state changes.
   *
   * @param {TranslationsLanguageState} languageState
   */
  #updateViewFromTranslationStatus(
    languageState = this.#getTranslationsActor().languageState
  ) {
    const { translateButton, toMenuList, fromMenuList, header, cancelButton } =
      this.elements;
    const { requestedTranslationPair, isEngineReady } = languageState;

    if (
      requestedTranslationPair &&
      !isEngineReady &&
      toMenuList.value === requestedTranslationPair.toLanguage &&
      fromMenuList.value === requestedTranslationPair.fromLanguage
    ) {
      // A translation has been requested, but is not ready yet.
      document.l10n.setAttributes(
        translateButton,
        "translations-panel-translate-button-loading"
      );
      translateButton.disabled = true;
      cancelButton.hidden = false;
      this.updateUIForReTranslation(false /* isReTranslation */);
    } else {
      document.l10n.setAttributes(
        translateButton,
        "translations-panel-translate-button"
      );
      translateButton.disabled =
        // The translation languages are the same, don't allow this translation.
        toMenuList.value === fromMenuList.value ||
        // No "to" language was provided.
        !toMenuList.value ||
        // No "from" language was provided.
        !fromMenuList.value ||
        // This is the requested translation pair.
        (requestedTranslationPair &&
          requestedTranslationPair.fromLanguage === fromMenuList.value &&
          requestedTranslationPair.toLanguage === toMenuList.value);
    }

    if (requestedTranslationPair && isEngineReady) {
      const { fromLanguage, toLanguage } = requestedTranslationPair;
      const displayNames = new Services.intl.DisplayNames(undefined, {
        type: "language",
      });
      cancelButton.hidden = true;
      this.updateUIForReTranslation(true /* isReTranslation */);

      document.l10n.setAttributes(header, "translations-panel-revisit-header", {
        fromLanguage: displayNames.of(fromLanguage),
        toLanguage: displayNames.of(toLanguage),
      });
    } else {
      document.l10n.setAttributes(header, "translations-panel-header");
    }
  }

  /**
   * @param {boolean} isReTranslation
   */
  updateUIForReTranslation(isReTranslation) {
    const { restoreButton, fromLabel, fromMenuList, toLabel } = this.elements;
    restoreButton.hidden = !isReTranslation;
    // When offering to re-translate a page, hide the "from" language so users don't
    // get confused.
    fromLabel.hidden = isReTranslation;
    fromMenuList.hidden = isReTranslation;
    if (isReTranslation) {
      fromLabel.style.marginBlockStart = "";
      toLabel.style.marginBlockStart = 0;
    } else {
      fromLabel.style.marginBlockStart = 0;
      toLabel.style.marginBlockStart = "";
    }
  }

  /**
   * Returns true if the panel is currently showing the default view, otherwise false.
   *
   * @returns {boolean}
   */
  #isShowingDefaultView() {
    if (!this.#lazyElements) {
      // Nothing has been initialized.
      return false;
    }
    const { multiview } = this.elements;
    return (
      multiview.getAttribute("mainViewId") === "translations-panel-view-default"
    );
  }

  /**
   * Show the default view of choosing a source and target language.
   *
   * @param {TranslationsParent} actor
   * @param {boolean} force - Force the page to show translation options.
   */
  async #showDefaultView(actor, force = false) {
    const {
      fromMenuList,
      multiview,
      panel,
      error,
      toMenuList,
      translateButton,
      langSelection,
      intro,
      header,
    } = this.elements;

    this.#updateViewFromTranslationStatus();

    // Unconditionally hide the intro text in case the panel is re-shown.
    intro.hidden = true;

    if (this.#langListsPhase === "error") {
      // There was an error, display it in the view rather than the language
      // dropdowns.
      const { cancelButton, errorHintAction } = this.elements;

      this.#showError({
        message: "translations-panel-error-load-languages",
        hint: "translations-panel-error-load-languages-hint",
        actionText: "translations-panel-error-load-languages-hint-button",
        actionCommand: () => this.#reloadLangList(actor),
      });

      translateButton.disabled = true;
      this.updateUIForReTranslation(false /* isReTranslation */);
      cancelButton.hidden = false;
      langSelection.hidden = true;
      errorHintAction.disabled = false;
      return;
    }

    // Remove any old selected values synchronously before asking for new ones.
    fromMenuList.value = "";
    error.hidden = true;
    langSelection.hidden = false;

    /** @type {null | LangTags} */
    const langTags = await this.#fetchDetectedLanguages();
    if (langTags?.isDocLangTagSupported || force) {
      // Show the default view with the language selection
      const { cancelButton } = this.elements;

      if (langTags?.isDocLangTagSupported) {
        fromMenuList.value = langTags?.docLangTag ?? "";
      } else {
        fromMenuList.value = "";
      }
      toMenuList.value = langTags?.userLangTag ?? "";

      this.onChangeLanguages();

      this.updateUIForReTranslation(false /* isReTranslation */);
      cancelButton.hidden = false;
      multiview.setAttribute("mainViewId", "translations-panel-view-default");

      if (!this._hasShownPanel) {
        actor.firstShowUriSpec = gBrowser.currentURI.spec;
      }

      if (
        this._hasShownPanel &&
        gBrowser.currentURI.spec !== actor.firstShowUriSpec
      ) {
        document.l10n.setAttributes(header, "translations-panel-header");
        actor.firstShowUriSpec = null;
        intro.hidden = true;
      } else {
        Services.prefs.setBoolPref("browser.translations.panelShown", true);
        intro.hidden = false;
        document.l10n.setAttributes(header, "translations-panel-intro-header");
      }
    } else {
      // Show the "unsupported language" view.
      const { unsupportedHint } = this.elements;
      multiview.setAttribute(
        "mainViewId",
        "translations-panel-view-unsupported-language"
      );
      let language;
      if (langTags?.docLangTag) {
        const displayNames = new Intl.DisplayNames(undefined, {
          type: "language",
          fallback: "none",
        });
        language = displayNames.of(langTags.docLangTag);
      }
      if (language) {
        document.l10n.setAttributes(
          unsupportedHint,
          "translations-panel-error-unsupported-hint-known",
          { language }
        );
      } else {
        document.l10n.setAttributes(
          unsupportedHint,
          "translations-panel-error-unsupported-hint-unknown"
        );
      }
    }

    // Focus the "from" language, as it is the only field not set.
    panel.addEventListener(
      "ViewShown",
      () => {
        if (!fromMenuList.value) {
          fromMenuList.focus();
        }
        if (!toMenuList.value) {
          toMenuList.focus();
        }
      },
      { once: true }
    );
  }

  /**
   * Updates the checked states of the settings menu checkboxes that
   * pertain to languages.
   */
  async #updateSettingsMenuLanguageCheckboxStates() {
    const langTags = await this.#getCachedDetectedLanguages();
    const { docLangTag, isDocLangTagSupported } = langTags;

    const { panel } = this.elements;
    const alwaysTranslateMenuItems = panel.ownerDocument.querySelectorAll(
      ".always-translate-language-menuitem"
    );
    const neverTranslateMenuItems = panel.ownerDocument.querySelectorAll(
      ".never-translate-language-menuitem"
    );
    const alwaysOfferTranslationsMenuItems =
      panel.ownerDocument.querySelectorAll(
        ".always-offer-translations-menuitem"
      );

    const alwaysOfferTranslations =
      TranslationsParent.shouldAlwaysOfferTranslations();
    const alwaysTranslateLanguage =
      TranslationsParent.shouldAlwaysTranslateLanguage(langTags);
    const neverTranslateLanguage =
      TranslationsParent.shouldNeverTranslateLanguage(docLangTag);
    const shouldDisable =
      !docLangTag ||
      !isDocLangTagSupported ||
      docLangTag === new Intl.Locale(Services.locale.appLocaleAsBCP47).language;

    for (const menuitem of alwaysOfferTranslationsMenuItems) {
      menuitem.setAttribute(
        "checked",
        alwaysOfferTranslations ? "true" : "false"
      );
    }
    for (const menuitem of alwaysTranslateMenuItems) {
      menuitem.setAttribute(
        "checked",
        alwaysTranslateLanguage ? "true" : "false"
      );
      menuitem.disabled = shouldDisable;
    }
    for (const menuitem of neverTranslateMenuItems) {
      menuitem.setAttribute(
        "checked",
        neverTranslateLanguage ? "true" : "false"
      );
      menuitem.disabled = shouldDisable;
    }
  }

  /**
   * Updates the checked states of the settings menu checkboxes that
   * pertain to site permissions.
   */
  async #updateSettingsMenuSiteCheckboxStates() {
    const { panel } = this.elements;
    const neverTranslateSiteMenuItems = panel.ownerDocument.querySelectorAll(
      ".never-translate-site-menuitem"
    );
    const neverTranslateSite =
      await this.#getTranslationsActor().shouldNeverTranslateSite();

    for (const menuitem of neverTranslateSiteMenuItems) {
      menuitem.setAttribute("checked", neverTranslateSite ? "true" : "false");
    }
  }

  /**
   * Populates the language-related settings menuitems by adding the
   * localized display name of the document's detected language tag.
   */
  async #populateSettingsMenuItems() {
    const { docLangTag } = await this.#getCachedDetectedLanguages();

    const { panel } = this.elements;

    const alwaysTranslateMenuItems = panel.ownerDocument.querySelectorAll(
      ".always-translate-language-menuitem"
    );
    const neverTranslateMenuItems = panel.ownerDocument.querySelectorAll(
      ".never-translate-language-menuitem"
    );

    /** @type {string | undefined} */
    let docLangDisplayName;
    if (docLangTag) {
      const displayNames = new Services.intl.DisplayNames(undefined, {
        type: "language",
        fallback: "none",
      });
      // The display name will still be empty if the docLangTag is not known.
      docLangDisplayName = displayNames.of(docLangTag);
    }

    for (const menuitem of alwaysTranslateMenuItems) {
      if (docLangDisplayName) {
        document.l10n.setAttributes(
          menuitem,
          "translations-panel-settings-always-translate-language",
          { language: docLangDisplayName }
        );
      } else {
        document.l10n.setAttributes(
          menuitem,
          "translations-panel-settings-always-translate-unknown-language"
        );
      }
    }

    for (const menuitem of neverTranslateMenuItems) {
      if (docLangDisplayName) {
        document.l10n.setAttributes(
          menuitem,
          "translations-panel-settings-never-translate-language",
          { language: docLangDisplayName }
        );
      } else {
        document.l10n.setAttributes(
          menuitem,
          "translations-panel-settings-never-translate-unknown-language"
        );
      }
    }

    await Promise.all([
      this.#updateSettingsMenuLanguageCheckboxStates(),
      this.#updateSettingsMenuSiteCheckboxStates(),
    ]);
  }

  /**
   * Configures the panel for the user to reset the page after it has been translated.
   *
   * @param {TranslationPair} translationPair
   */
  async #showRevisitView({ fromLanguage, toLanguage }) {
    const { fromMenuList, toMenuList, intro } = this.elements;
    if (!this.#isShowingDefaultView()) {
      await this.#showDefaultView(this.#getTranslationsActor());
    }
    intro.hidden = true;
    fromMenuList.value = fromLanguage;
    toMenuList.value = toLanguage;
    this.onChangeLanguages();
  }

  /**
   * Handle the disable logic for when the menulist is changed for the "Translate to"
   * on the "revisit" subview.
   */
  onChangeRevisitTo() {
    const { revisitTranslate, revisitMenuList } = this.elements;
    revisitTranslate.disabled = !revisitMenuList.value;
  }

  /**
   * Handle logic and telemetry for changing the selected from-language option.
   *
   * @param {Event} event
   */
  onChangeFromLanguage(event) {
    const { target } = event;
    if (target?.value) {
      TranslationsParent.telemetry().panel().onChangeFromLanguage(target.value);
    }
    this.onChangeLanguages();
  }

  /**
   * Handle logic and telemetry for changing the selected to-language option.
   *
   * @param {Event} event
   */
  onChangeToLanguage(event) {
    const { target } = event;
    if (target?.value) {
      TranslationsParent.telemetry().panel().onChangeToLanguage(target.value);
    }
    this.onChangeLanguages();
  }

  /**
   * When changing the language selection, the translate button will need updating.
   */
  onChangeLanguages() {
    this.#updateViewFromTranslationStatus();
  }

  /**
   * Hide the pop up (for event handlers).
   */
  close() {
    PanelMultiView.hidePopup(this.elements.panel);
  }

  /*
   * Handler for clicking the learn more link from linked text
   * within the translations panel.
   */
  onLearnMoreLink() {
    TranslationsParent.telemetry().panel().onLearnMoreLink();
    TranslationsPanel.close();
  }

  /*
   * Handler for clicking the learn more link from the gear menu.
   */
  onAboutTranslations() {
    TranslationsParent.telemetry().panel().onAboutTranslations();
    PanelMultiView.hidePopup(this.elements.panel);
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
   * When a language is not supported and the menu is manually invoked, an error message
   * is shown. This method switches the panel back to the language selection view.
   * Note that this bypasses the showSubView method since the main view doesn't support
   * a subview.
   */
  async onChangeSourceLanguage(event) {
    const { panel } = this.elements;
    PanelMultiView.hidePopup(panel);

    await this.#showDefaultView(
      this.#getTranslationsActor(),
      true /* force this view to be shown */
    );

    await this.#openPanelPopup(this.elements.appMenuButton, {
      event,
      viewName: "defaultView",
      maintainFlow: true,
    });
  }

  /**
   * @param {TranslationsActor} actor
   */
  async #reloadLangList(actor) {
    try {
      await this.#ensureLangListsBuilt();
      await this.#showDefaultView(actor);
    } catch (error) {
      this.elements.errorHintAction.disabled = false;
    }
  }

  /**
   * Handle telemetry events when buttons are invoked in the panel.
   *
   * @param {Event} event
   */
  handlePanelButtonEvent(event) {
    const {
      cancelButton,
      changeSourceLanguageButton,
      dismissErrorButton,
      restoreButton,
      translateButton,
    } = this.elements;
    switch (event.target.id) {
      case cancelButton.id: {
        TranslationsParent.telemetry().panel().onCancelButton();
        break;
      }
      case changeSourceLanguageButton.id: {
        TranslationsParent.telemetry().panel().onChangeSourceLanguageButton();
        break;
      }
      case dismissErrorButton.id: {
        TranslationsParent.telemetry().panel().onDismissErrorButton();
        break;
      }
      case restoreButton.id: {
        TranslationsParent.telemetry().panel().onRestorePageButton();
        break;
      }
      case translateButton.id: {
        TranslationsParent.telemetry().panel().onTranslateButton();
        break;
      }
    }
  }

  /**
   * Handle telemetry events when popups are shown in the panel.
   *
   * @param {Event} event
   */
  handlePanelPopupShownEvent(event) {
    const { panel, fromMenuList, toMenuList } = this.elements;
    switch (event.target.id) {
      case panel.id: {
        // This telemetry event is invoked externally because it requires
        // extra logic about from where the panel was opened and whether
        // or not the flow should be maintained or started anew.
        break;
      }
      case fromMenuList.firstChild.id: {
        TranslationsParent.telemetry().panel().onOpenFromLanguageMenu();
        break;
      }
      case toMenuList.firstChild.id: {
        TranslationsParent.telemetry().panel().onOpenToLanguageMenu();
        break;
      }
    }
  }

  /**
   * Handle telemetry events when popups are hidden in the panel.
   *
   * @param {Event} event
   */
  handlePanelPopupHiddenEvent(event) {
    const { panel, fromMenuList, toMenuList } = this.elements;
    switch (event.target.id) {
      case panel.id: {
        TranslationsParent.telemetry().panel().onClose();
        this.#isPopupOpen = false;
        this.elements.error.hidden = true;
        break;
      }
      case fromMenuList.firstChild.id: {
        TranslationsParent.telemetry().panel().onCloseFromLanguageMenu();
        break;
      }
      case toMenuList.firstChild.id: {
        TranslationsParent.telemetry().panel().onCloseToLanguageMenu();
        break;
      }
    }
  }

  /**
   * Handle telemetry events when the settings menu is shown.
   */
  handleSettingsPopupShownEvent() {
    TranslationsParent.telemetry().panel().onOpenSettingsMenu();
  }

  /**
   * Handle telemetry events when the settings menu is hidden.
   */
  handleSettingsPopupHiddenEvent() {
    TranslationsParent.telemetry().panel().onCloseSettingsMenu();
  }

  /**
   * Opens the Translations panel popup at the given target.
   *
   * @param {object} target - The target element at which to open the popup.
   * @param {object} telemetryData
   * @param {string} telemetryData.event
   *   The trigger event for opening the popup.
   * @param {string} telemetryData.viewName
   *   The name of the view shown by the panel.
   * @param {boolean} telemetryData.autoShow
   *   True if the panel was automatically opened, otherwise false.
   * @param {boolean} telemetryData.maintainFlow
   *   Whether or not to maintain the flow of telemetry.
   * @param {boolean} telemetryData.isFirstUserInteraction
   *   Whether or not this is the first user interaction with the panel.
   */
  async #openPanelPopup(
    target,
    {
      event = null,
      viewName = null,
      autoShow = false,
      maintainFlow = false,
      isFirstUserInteraction = null,
    }
  ) {
    await window.ensureCustomElements("moz-button-group");

    const { panel, appMenuButton } = this.elements;
    const openedFromAppMenu = target.id === appMenuButton.id;
    const { docLangTag } = await this.#getCachedDetectedLanguages();

    TranslationsParent.telemetry().panel().onOpen({
      viewName,
      autoShow,
      docLangTag,
      maintainFlow,
      openedFromAppMenu,
      isFirstUserInteraction,
    });

    this.#isPopupOpen = true;

    PanelMultiView.openPopup(panel, target, {
      position: "bottomright topright",
      triggerEvent: event,
    }).catch(error => this.console?.error(error));
  }

  /**
   * Keeps track of open requests to guard against race conditions.
   *
   * @type {Promise<void> | null}
   */
  #openPromise = null;

  /**
   * Opens the TranslationsPanel.
   *
   * @param {Event} event
   * @param {boolean} reportAsAutoShow
   *   True to report to telemetry that the panel was opened automatically, otherwise false.
   */
  async open(event, reportAsAutoShow = false) {
    if (this.#openPromise) {
      // There is already an open event happening, do not open.
      return;
    }

    this.#openPromise = this.#openImpl(event, reportAsAutoShow);
    this.#openPromise.finally(() => {
      this.#openPromise = null;
    });
  }

  /**
   * Implementation function for opening the panel. Prefer TranslationsPanel.open.
   *
   * @param {Event} event
   */
  async #openImpl(event, reportAsAutoShow) {
    event.stopPropagation();
    if (
      (event.type == "click" && event.button != 0) ||
      (event.type == "keypress" &&
        event.charCode != KeyEvent.DOM_VK_SPACE &&
        event.keyCode != KeyEvent.DOM_VK_RETURN)
    ) {
      // Allow only left click, space, or enter.
      return;
    }

    const window =
      gBrowser.selectedBrowser.browsingContext.top.embedderElement.ownerGlobal;
    window.ensureCustomElements("moz-support-link");

    const { button } = this.buttonElements;

    const { requestedTranslationPair, locationChangeId } =
      this.#getTranslationsActor().languageState;

    // Store this value because it gets modified when #showDefaultView is called below.
    const isFirstUserInteraction = !this._hasShownPanel;

    await this.#ensureLangListsBuilt();

    if (requestedTranslationPair) {
      await this.#showRevisitView(requestedTranslationPair).catch(error => {
        this.console?.error(error);
      });
    } else {
      await this.#showDefaultView(this.#getTranslationsActor()).catch(error => {
        this.console?.error(error);
      });
    }

    this.#populateSettingsMenuItems();

    const targetButton =
      button.contains(event.target) ||
      event.type === "TranslationsParent:OfferTranslation"
        ? button
        : this.elements.appMenuButton;

    if (!TranslationsParent.isActiveLocation(locationChangeId)) {
      this.console?.log(`A translation panel open request was stale.`, {
        locationChangeId,
        newlocationChangeId:
          this.#getTranslationsActor().languageState.locationChangeId,
        currentURISpec: gBrowser.currentURI.spec,
      });
      return;
    }

    this.console?.log(`Showing a translation panel`, gBrowser.currentURI.spec);

    await this.#openPanelPopup(targetButton, {
      event,
      autoShow: reportAsAutoShow,
      viewName: requestedTranslationPair ? "revisitView" : "defaultView",
      maintainFlow: false,
      isFirstUserInteraction,
    });
  }

  /**
   * Returns true if translations is currently active, otherwise false.
   *
   * @returns {boolean}
   */
  #isTranslationsActive() {
    const { requestedTranslationPair } =
      this.#getTranslationsActor().languageState;
    return requestedTranslationPair !== null;
  }

  /**
   * Handle the translation button being clicked when there are two language options.
   */
  async onTranslate() {
    PanelMultiView.hidePopup(this.elements.panel);

    const actor = this.#getTranslationsActor();
    actor.translate(
      this.elements.fromMenuList.value,
      this.elements.toMenuList.value,
      false // reportAsAutoTranslate
    );
  }

  /**
   * Handle the cancel button being clicked.
   */
  onCancel() {
    PanelMultiView.hidePopup(this.elements.panel);
  }

  /**
   * A handler for opening the settings context menu.
   */
  openSettingsPopup(button) {
    this.#updateSettingsMenuLanguageCheckboxStates();
    this.#updateSettingsMenuSiteCheckboxStates();
    const popup = button.ownerDocument.getElementById(
      "translations-panel-settings-menupopup"
    );
    popup.openPopup(button, "after_end");
  }

  /**
   * Creates a new CheckboxPageAction based on the current translated
   * state of the page and the state of the persistent options in the
   * translations panel settings.
   *
   * @returns {CheckboxPageAction}
   */
  getCheckboxPageActionFor() {
    const {
      alwaysTranslateLanguageMenuItem,
      neverTranslateLanguageMenuItem,
      neverTranslateSiteMenuItem,
    } = this.elements;

    const alwaysTranslateLanguage =
      alwaysTranslateLanguageMenuItem.getAttribute("checked") === "true";
    const neverTranslateLanguage =
      neverTranslateLanguageMenuItem.getAttribute("checked") === "true";
    const neverTranslateSite =
      neverTranslateSiteMenuItem.getAttribute("checked") === "true";

    return new CheckboxPageAction(
      this.#isTranslationsActive(),
      alwaysTranslateLanguage,
      neverTranslateLanguage,
      neverTranslateSite
    );
  }

  /**
   * Redirect the user to about:preferences
   */
  openManageLanguages() {
    TranslationsParent.telemetry().panel().onManageLanguages();
    const window =
      gBrowser.selectedBrowser.browsingContext.top.embedderElement.ownerGlobal;
    window.openTrustedLinkIn("about:preferences#general-translations", "tab");
  }

  /**
   * Performs the given page action.
   *
   * @param {PageAction} pageAction
   */
  async #doPageAction(pageAction) {
    switch (pageAction) {
      case PageAction.NO_CHANGE: {
        break;
      }
      case PageAction.RESTORE_PAGE: {
        await this.onRestore();
        break;
      }
      case PageAction.TRANSLATE_PAGE: {
        await this.onTranslate();
        break;
      }
    }
  }

  /**
   * Updates the always-translate-language menuitem prefs and checked state.
   * If auto-translate is currently active for the doc language, deactivates it.
   * If auto-translate is currently inactive for the doc language, activates it.
   */
  async onAlwaysTranslateLanguage() {
    const langTags = await this.#getCachedDetectedLanguages();
    const { docLangTag } = langTags;
    if (!docLangTag) {
      throw new Error("Expected to have a document language tag.");
    }
    const pageAction =
      this.getCheckboxPageActionFor().alwaysTranslateLanguage();
    const toggledOn =
      TranslationsParent.toggleAlwaysTranslateLanguagePref(langTags);
    TranslationsParent.telemetry()
      .panel()
      .onAlwaysTranslateLanguage(docLangTag, toggledOn);
    this.#updateSettingsMenuLanguageCheckboxStates();
    await this.#doPageAction(pageAction);
  }

  /**
   * Toggle offering translations.
   */
  async onAlwaysOfferTranslations() {
    const toggledOn = TranslationsParent.toggleAutomaticallyPopupPref();
    TranslationsParent.telemetry().panel().onAlwaysOfferTranslations(toggledOn);
  }

  /**
   * Updates the never-translate-language menuitem prefs and checked state.
   * If never-translate is currently active for the doc language, deactivates it.
   * If never-translate is currently inactive for the doc language, activates it.
   */
  async onNeverTranslateLanguage() {
    const { docLangTag } = await this.#getCachedDetectedLanguages();
    if (!docLangTag) {
      throw new Error("Expected to have a document language tag.");
    }
    const pageAction = this.getCheckboxPageActionFor().neverTranslateLanguage();
    const toggledOn =
      TranslationsParent.toggleNeverTranslateLanguagePref(docLangTag);
    TranslationsParent.telemetry()
      .panel()
      .onNeverTranslateLanguage(docLangTag, toggledOn);
    this.#updateSettingsMenuLanguageCheckboxStates();
    await this.#doPageAction(pageAction);
  }

  /**
   * Updates the never-translate-site menuitem permissions and checked state.
   * If never-translate is currently active for the site, deactivates it.
   * If never-translate is currently inactive for the site, activates it.
   */
  async onNeverTranslateSite() {
    const pageAction = this.getCheckboxPageActionFor().neverTranslateSite();
    const toggledOn =
      await this.#getTranslationsActor().toggleNeverTranslateSitePermissions();
    TranslationsParent.telemetry().panel().onNeverTranslateSite(toggledOn);
    this.#updateSettingsMenuSiteCheckboxStates();
    await this.#doPageAction(pageAction);
  }

  /**
   * Handle the restore button being clicked.
   */
  async onRestore() {
    const { panel } = this.elements;
    PanelMultiView.hidePopup(panel);
    const { docLangTag } = await this.#getCachedDetectedLanguages();
    if (!docLangTag) {
      throw new Error("Expected to have a document language tag.");
    }

    this.#getTranslationsActor().restorePage(docLangTag);
  }

  /**
   * An event handler that allows the TranslationsPanel object
   * to be compatible with the addTabsProgressListener function.
   *
   * @param {tabbrowser} browser
   */
  onLocationChange(browser) {
    if (browser.currentURI.spec.startsWith("about:reader")) {
      // Hide the translations button when entering reader mode.
      this.buttonElements.button.hidden = true;
    }
  }

  /**
   * Chain together the handleEvent calls so that they always run sequentially to guard
   * against race conditions.
   *
   * @type {Promise<void>}
   */
  handleEventChain = Promise.resolve();

  /**
   * Handle the chaining
   *
   * @param {CustomEvent} event
   */
  handleEvent = event => {
    // The events are handled asynchronously, and chained together. Because of this,
    // the handleEventImpl code may not be firing for an actor that is part of the
    // current gBrowser. Get a reference to the translations actor associated with this
    // event, and then any methods called from here need to reference this actor.
    // This will prevent race conditions in the handler.
    let actor;
    try {
      actor =
        event.target.browsingContext.currentWindowGlobal.getActor(
          "Translations"
        );
    } catch {}

    if (actor) {
      this.handleEventChain = this.handleEventChain
        .catch(() => {})
        .then(() => this.handleEventImpl(event, actor));
    } else {
      this.console?.error(
        `Unable to get Translations actor for event "${event.type}"`,
        event
      );
    }
    return this.handleEventChain;
  };

  /**
   * Set the state of the translations button in the URL bar.
   *
   * @param {CustomEvent} event
   */
  async handleEventImpl(event, actor) {
    switch (event.type) {
      case "TranslationsParent:OfferTranslation": {
        if (Services.wm.getMostRecentBrowserWindow()?.gBrowser === gBrowser) {
          this.open(event, /* reportAsAutoShow */ true);
        }
        break;
      }
      case "TranslationsParent:LanguageState": {
        const {
          detectedLanguages,
          requestedTranslationPair,
          error,
          isEngineReady,
        } = event.detail;

        const { button, buttonLocale, buttonCircleArrows } =
          this.buttonElements;

        const hasSupportedLanguage =
          detectedLanguages?.docLangTag &&
          detectedLanguages?.userLangTag &&
          detectedLanguages?.isDocLangTagSupported;

        if (detectedLanguages) {
          // Ensure the cached detected languages are up to date, for instance whenever
          // the user switches tabs.
          TranslationsPanel.detectedLanguages = detectedLanguages;
        }

        if (this.#isPopupOpen) {
          // Make sure to use the language state that is passed by the event.detail, and
          // don't read it from the actor here, as it's possible the actor isn't available
          // via the gBrowser.selectedBrowser.
          this.#updateViewFromTranslationStatus(event.detail);
        }

        if (
          // We've already requested to translate this page, so always show the icon.
          requestedTranslationPair ||
          // There was an error translating, so always show the icon. This can happen
          // when a user manually invokes the translation and we wouldn't normally show
          // the icon.
          error ||
          // Finally check that we can translate this language.
          (hasSupportedLanguage &&
            (await TranslationsParent.getIsTranslationsEngineSupported()))
        ) {
          // Keep track if the button was originally hidden, because it will be shown now.
          const wasButtonHidden = button.hidden;

          button.hidden = false;
          if (requestedTranslationPair) {
            // The translation is active, update the urlbar button.
            button.setAttribute("translationsactive", true);
            if (isEngineReady) {
              const displayNames = new Services.intl.DisplayNames(undefined, {
                type: "language",
              });

              document.l10n.setAttributes(
                button,
                "urlbar-translations-button-translated",
                {
                  fromLanguage: displayNames.of(
                    requestedTranslationPair.fromLanguage
                  ),
                  toLanguage: displayNames.of(
                    requestedTranslationPair.toLanguage
                  ),
                }
              );
              // Show the locale of the page in the button.
              buttonLocale.hidden = false;
              buttonCircleArrows.hidden = true;
              buttonLocale.innerText = requestedTranslationPair.toLanguage;
            } else {
              document.l10n.setAttributes(
                button,
                "urlbar-translations-button-loading"
              );
              // Show the spinning circle arrows to indicate that the engine is
              // still loading.
              buttonCircleArrows.hidden = false;
              buttonLocale.hidden = true;
            }
          } else {
            // The translation is not active, update the urlbar button.
            button.removeAttribute("translationsactive");
            buttonLocale.hidden = true;
            buttonCircleArrows.hidden = true;

            // Follow the same rules for displaying the first-run intro text for the
            // button's accessible tooltip label.
            if (
              this._hasShownPanel &&
              gBrowser.currentURI.spec !== actor.firstShowUriSpec
            ) {
              document.l10n.setAttributes(
                button,
                "urlbar-translations-button2"
              );
            } else {
              document.l10n.setAttributes(
                button,
                "urlbar-translations-button-intro"
              );
            }
          }

          // The button was hidden, but now it is shown.
          if (wasButtonHidden) {
            PageActions.sendPlacedInUrlbarTrigger(button);
          }
        } else if (!button.hidden) {
          // There are no translations visible, hide the button.
          button.hidden = true;
        }

        switch (error) {
          case null:
            break;
          case "engine-load-failure":
            await this.#ensureLangListsBuilt();
            if (!this.#isShowingDefaultView()) {
              await this.#showDefaultView(actor).catch(e => {
                this.console?.error(e);
              });
            }
            this.elements.error.hidden = false;
            this.#showError({
              message: "translations-panel-error-translating",
            });
            const targetButton = button.hidden
              ? this.elements.appMenuButton
              : button;

            // Re-open the menu on an error.
            await this.#openPanelPopup(targetButton, {
              autoShow: true,
              viewName: "errorView",
              maintainFlow: true,
            });
            break;
          default:
            console.error("Unknown translation error", error);
        }
        break;
      }
    }
  }
})();

XPCOMUtils.defineLazyPreferenceGetter(
  TranslationsPanel,
  "_hasShownPanel",
  "browser.translations.panelShown",
  false
);
