/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

/* eslint-disable jsdoc/valid-types */
/**
 * @typedef {import("../../../../toolkit/components/translations/translations").LangTags} LangTags
 */
/* eslint-enable jsdoc/valid-types */

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
   * Lazily get a console instance.
   *
   * @returns {Console}
   */
  get console() {
    if (!this.#console) {
      this.#console = console.createInstance({
        maxLogLevelPref: "browser.translations.logLevel",
        prefix: "Translations",
      });
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
        header.appendChild(settingsButton.cloneNode(true));
      }

      // Lazily select the elements.
      this.#lazyElements = {
        panel,
        settingsButton,
        // The rest of the elements are set by the getter below.
      };

      /**
       * Define a getter on #lazyElements that gets the element by an id.
       */
      const getter = (name, id) => {
        let element;
        Object.defineProperty(this.#lazyElements, name, {
          get: () => {
            if (!element) {
              element = document.getElementById(id);
            }
            if (!element) {
              throw new Error(`Could not find "${name}" at "#${id}".`);
            }
            return element;
          },
        });
      };

      getter("button", "translations-button");
      getter("buttonLocale", "translations-button-locale");
      getter("buttonCircleArrows", "translations-button-circle-arrows");
      getter("defaultDescription", "translations-panel-default-description");
      getter("defaultToMenuList", "translations-panel-default-to");
      getter("dualFromMenuList", "translations-panel-dual-from");
      getter("dualToMenuList", "translations-panel-dual-to");
      getter("dualTranslate", "translations-panel-dual-translate");
      getter("error", "translations-panel-error");
      getter("errorMessage", "translations-panel-error-message");
      getter("multiview", "translations-panel-multiview");
      getter("notNow", "translations-panel-not-now");
      getter("revisitHeader", "translations-panel-revisit-header");
      getter("revisitMenuList", "translations-panel-revisit-to");
      getter("revisitTranslate", "translations-panel-revisit-translate");
    }

    return this.#lazyElements;
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
      await this.#getTranslationsActor().getLangTagsForTranslation();
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
        this.console.error("Unknown langList phase", this.#langListsPhase);
    }

    try {
      /** @type {SupportedLanguages} */
      const { languagePairs, fromLanguages, toLanguages } =
        await this.#getTranslationsActor().getSupportedLanguages();

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
        for (const { langTag, isBeta, displayName } of fromLanguages) {
          const fromMenuItem = document.createXULElement("menuitem");
          fromMenuItem.setAttribute("value", langTag);
          if (isBeta) {
            document.l10n.setAttributes(
              fromMenuItem,
              "translations-panel-displayname-beta",
              { language: displayName }
            );
          } else {
            fromMenuItem.setAttribute("label", displayName);
          }
          popup.appendChild(fromMenuItem);
        }
      }

      for (const popup of toPopups) {
        for (const { langTag, isBeta, displayName } of toLanguages) {
          const toMenuItem = document.createXULElement("menuitem");
          toMenuItem.setAttribute("value", langTag);
          if (isBeta) {
            document.l10n.setAttributes(
              toMenuItem,
              "translations-panel-displayname-beta",
              { language: displayName }
            );
          } else {
            toMenuItem.setAttribute("label", displayName);
          }
          popup.appendChild(toMenuItem);
        }
      }

      this.#langListsPhase = "initialized";
    } catch (error) {
      this.console.error(error);
      this.#langListsPhase = "error";
    }
  }

  /**
   * Switch to the dual language view of choosing a source and target language.
   */
  showDualView() {
    const {
      dualTranslate,
      dualFromMenuList,
      dualToMenuList,
      multiview,
      defaultToMenuList,
      panel,
    } = this.elements;

    // Remove any old selected values synchronously before asking for new ones.
    dualFromMenuList.value = "";
    dualToMenuList.value = defaultToMenuList.value;
    // Disable this button since the user must choose a new "from" language.
    dualTranslate.disabled = true;

    multiview.showSubView("translations-panel-view-dual");

    // Focus the "from" language, as it is the only field not set.
    panel.addEventListener(
      "ViewShown",
      () => {
        dualFromMenuList.focus();
      },
      { once: true }
    );
  }

  /**
   * Builds the <menulist> of languages for both the "from" and "to". This can be
   * called every time the popup is shown, as it will retry when there is an error
   * (such as a network error) or be a noop if it's already initialized.
   */
  async #showDefaultView() {
    await this.#ensureLangListsBuilt();
    const actor = this.#getTranslationsActor();

    const { defaultToMenuList, defaultDescription, multiview } = this.elements;

    multiview.setAttribute("mainViewId", "translations-panel-view-default");

    // Remove any old selected values synchronously before asking for new ones.
    defaultToMenuList.value = "";

    // TODO(Bug 1825801) - There is a race condition, we may download the languages, and
    // later trigger the subview to be shown after opening the popup again. We need to
    // properly handle this.

    // TODO(Bug 1825801) - This could potentially be a bad pause, as we aren't showing
    // the panel until the language list is ready. It's probably fine for a prototype,
    // but should be handled for the MVP. We might want design direction here, as we need
    // a subview for when the language list is still being retrieved.

    /** @type {null | LangTags} */
    const langTags = await actor.getLangTagsForTranslation();

    if (langTags) {
      const { docLangTag, appLangTag } = langTags;
      defaultToMenuList.value = appLangTag;
      this.#docLangTag = docLangTag;
    } else {
      // TODO(Bug 1829687): Handle the case when we don't have the document langauge tag
      // which can only be triggered when the panel is shown manually. Currently
      // this will never be shown.
      this.#docLangTag = "en";
      this.console.error("No language tags for translation were found.");
    }

    // Show the default view.
    const displayNames = new Services.intl.DisplayNames(undefined, {
      type: "language",
    });
    document.l10n.setAttributes(
      defaultDescription,
      "translations-panel-default-description",
      {
        pageLanguage: displayNames.of(this.#docLangTag),
      }
    );

    if (!this.#wasPanelShown) {
      // Note if a profile has used translations before, we may want to include additional
      // messaging for first time users.
      this.#wasPanelShown = true;
      Services.prefs.setBoolPref("browser.translations.panel.wasShown", true);
    }

    for (const menuitem of defaultToMenuList.querySelectorAll("menuitem")) {
      // It is not valid to translate into the original doc language.
      menuitem.disabled = menuitem.value === this.#docLangTag;
    }
  }

  /**
   * Updates the checked states of the settings menu checkboxes that
   * pertain to languages.
   */
  async #updateSettingsMenuLanguageCheckboxStates() {
    const { docLangTag, isDocLangTagSupported } =
      await this.#getCachedDetectedLanguages();

    const { panel } = this.elements;
    const alwaysTranslateMenuItems = panel.querySelectorAll(
      ".always-translate-language-menuitem"
    );
    const neverTranslateMenuItems = panel.querySelectorAll(
      ".never-translate-language-menuitem"
    );

    if (
      !docLangTag ||
      !isDocLangTagSupported ||
      docLangTag === new Intl.Locale(Services.locale.appLocaleAsBCP47).language
    ) {
      for (const menuitem of alwaysTranslateMenuItems) {
        menuitem.disabled = true;
      }
      for (const menuitem of neverTranslateMenuItems) {
        menuitem.disabled = true;
      }
      return;
    }

    const alwaysTranslateLanguage =
      TranslationsParent.shouldAlwaysTranslateLanguage(docLangTag);
    const neverTranslateLanguage =
      TranslationsParent.shouldNeverTranslateLanguage(docLangTag);

    for (const menuitem of alwaysTranslateMenuItems) {
      menuitem.setAttribute(
        "checked",
        alwaysTranslateLanguage ? "true" : "false"
      );
      menuitem.disabled = false;
    }
    for (const menuitem of neverTranslateMenuItems) {
      menuitem.setAttribute(
        "checked",
        neverTranslateLanguage ? "true" : "false"
      );
      menuitem.disabled = false;
    }
  }

  /**
   * Updates the checked states of the settings menu checkboxes that
   * pertain to site permissions.
   */
  async #updateSettingsMenuSiteCheckboxStates() {
    const { panel } = this.elements;
    const neverTranslateSiteMenuItems = panel.querySelectorAll(
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

    const alwaysTranslateMenuItems = panel.querySelectorAll(
      ".always-translate-language-menuitem"
    );
    const neverTranslateMenuItems = panel.querySelectorAll(
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
    const { multiview, revisitHeader, revisitMenuList, revisitTranslate } =
      this.elements;

    await this.#ensureLangListsBuilt();

    revisitMenuList.value = "";
    revisitTranslate.disabled = true;
    multiview.setAttribute("mainViewId", "translations-panel-view-revisit");

    const displayNames = new Services.intl.DisplayNames(undefined, {
      type: "language",
    });

    for (const menuitem of revisitMenuList.querySelectorAll("menuitem")) {
      menuitem.disabled = menuitem.value === toLanguage;
    }

    document.l10n.setAttributes(
      revisitHeader,
      "translations-panel-revisit-header",
      {
        fromLanguage: displayNames.of(fromLanguage),
        toLanguage: displayNames.of(toLanguage),
      }
    );
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
   * When changing the "dual" view's language, handle cases where the translate button
   * should be disabled.
   */
  onChangeDualLanguages() {
    const { dualTranslate, dualToMenuList, dualFromMenuList } = this.elements;
    dualTranslate.disabled =
      // The translation languages are the same, don't allow this translation.
      dualToMenuList.value === dualFromMenuList.value ||
      // No "from" language was provided.
      !dualFromMenuList.value;
  }

  /**
   * Opens the TranslationsPanel.
   *
   * @param {Event} event
   */
  async open(event) {
    const { panel, button } = this.elements;

    const { requestedTranslationPair } =
      this.#getTranslationsActor().languageState;

    if (requestedTranslationPair) {
      await this.#showRevisitView(requestedTranslationPair).catch(error => {
        this.console.error(error);
      });
    } else {
      await this.#showDefaultView().catch(error => {
        this.console.error(error);
      });
    }

    this.#populateSettingsMenuItems();
    PanelMultiView.openPopup(panel, button, {
      position: "bottomright topright",
      triggerEvent: event,
    }).catch(error => this.console.error(error));
  }

  /**
   * Removes the translations button.
   */
  #hideTranslationsButton() {
    const { button, buttonLocale, buttonCircleArrows } = this.elements;
    button.hidden = true;
    buttonLocale.hidden = true;
    buttonCircleArrows.hidden = true;
    button.removeAttribute("translationsactive");
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
   * Handle the translation button being clicked on the default view.
   */
  async onDefaultTranslate() {
    PanelMultiView.hidePopup(this.elements.panel);

    const actor = this.#getTranslationsActor();
    const docLangTag = await this.#getDocLangTag();
    actor.translate(docLangTag, this.elements.defaultToMenuList.value);
  }

  /**
   * Handle the translation button being clicked when there are two language options.
   */
  async onDualTranslate() {
    PanelMultiView.hidePopup(this.elements.panel);

    const actor = this.#getTranslationsActor();
    actor.translate(
      this.elements.dualFromMenuList.value,
      this.elements.dualToMenuList.value
    );
  }

  /**
   * Handle the translation button being clicked when the page has already been
   * translated.
   */
  async onRevisitTranslate() {
    PanelMultiView.hidePopup(this.elements.panel);

    const actor = this.#getTranslationsActor();
    const docLangTag = await this.#getDocLangTag();
    actor.translate(docLangTag, this.elements.revisitMenuList.value);
  }

  onCancel() {
    PanelMultiView.hidePopup(this.elements.panel);
  }

  /**
   * A handler for opening the settings context menu.
   */
  openSettingsPopup(button) {
    this.#updateSettingsMenuLanguageCheckboxStates();
    this.#updateSettingsMenuSiteCheckboxStates();
    const popup = button.querySelector("menupopup");
    popup.openPopup(button);
  }

  /**
   * Redirect the user to about:preferences
   */
  openManageLanguages() {
    const window =
      gBrowser.selectedBrowser.browsingContext.top.embedderElement.ownerGlobal;
    window.openTrustedLinkIn("about:preferences#general-translations", "tab");
  }

  /**
   * Updates the always-translate-language menuitem prefs and checked state.
   * If auto-translate is currently active for the doc language, deactivates it.
   * If auto-translate is currently inactive for the doc language, activates it.
   */
  async onAlwaysTranslateLanguage() {
    const { docLangTag } = await this.#getCachedDetectedLanguages();
    if (!docLangTag) {
      throw new Error("Expected to have a document language tag.");
    }
    const toggledOn =
      TranslationsParent.toggleAlwaysTranslateLanguagePref(docLangTag);
    const translationsActive = this.#isTranslationsActive();
    this.#updateSettingsMenuLanguageCheckboxStates();

    if (toggledOn && !translationsActive) {
      await this.onDefaultTranslate();
    } else if (!toggledOn && translationsActive) {
      await this.onRestore();
    }

    // If always-translate was activated while translations is active
    // If always-translate was deactivated while translations is inactive
    // There is no need to change the state of the page.
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
    TranslationsParent.toggleNeverTranslateLanguagePref(docLangTag);
    this.#updateSettingsMenuLanguageCheckboxStates();

    if (this.#isTranslationsActive()) {
      await this.onRestore();
    } else {
      this.#hideTranslationsButton();
    }
  }

  /**
   * Updates the never-translate-site menuitem permissions and checked state.
   * If never-translate is currently active for the site, deactivates it.
   * If never-translate is currently inactive for the site, activates it.
   */
  async onNeverTranslateSite() {
    await this.#getTranslationsActor().toggleNeverTranslateSitePermissions();
    this.#updateSettingsMenuSiteCheckboxStates();

    if (this.#isTranslationsActive()) {
      await this.onRestore();
    } else {
      this.#hideTranslationsButton();
    }
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
   * Set the state of the translations button in the URL bar.
   *
   * @param {CustomEvent} event
   */
  handleEvent = async event => {
    switch (event.type) {
      case "TranslationsParent:LanguageState":
        const {
          detectedLanguages,
          requestedTranslationPair,
          error,
          isEngineReady,
        } = event.detail;

        const { panel, button, buttonLocale, buttonCircleArrows } =
          this.elements;

        const hasSupportedLanguage =
          detectedLanguages?.docLangTag &&
          detectedLanguages?.userLangTag &&
          detectedLanguages?.isDocLangTagSupported;

        if (detectedLanguages) {
          // Ensure the cached detected languages are up to date, for instance whenever
          // the user switches tabs.
          TranslationsPanel.detectedLanguages = detectedLanguages;
        }

        /**
         * Defer this check to the end of the `if` statement since it requires work.
         */
        const shouldNeverTranslate = async () => {
          return Boolean(
            TranslationsParent.shouldNeverTranslateLanguage(
              detectedLanguages?.docLangTag
            ) ||
              // The site is present in the never-translate list.
              (await this.#getTranslationsActor().shouldNeverTranslateSite())
          );
        };

        if (
          // We've already requested to translate this page, so always show the icon.
          requestedTranslationPair ||
          // There was an error translating, so always show the icon. This can happen
          // when a user manually invokes the translation and we wouldn't normally show
          // the icon.
          error ||
          // Finally check that this is a supported language that we should translate.
          (hasSupportedLanguage && !(await shouldNeverTranslate()))
        ) {
          button.hidden = false;
          if (requestedTranslationPair) {
            // The translation is active, update the urlbar button.
            button.setAttribute("translationsactive", true);
            if (isEngineReady) {
              // Show the locale of the page in the button.
              buttonLocale.hidden = false;
              buttonCircleArrows.hidden = true;
              buttonLocale.innerText = requestedTranslationPair.toLanguage;
            } else {
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
          }
        } else {
          this.#hideTranslationsButton();
        }

        switch (error) {
          case null:
            this.elements.error.hidden = true;
            this.elements.notNow.hidden = false;
            break;
          case "engine-load-failure":
            this.elements.error.hidden = false;
            this.elements.notNow.hidden = true;
            document.l10n.setAttributes(
              this.elements.errorMessage,
              "translations-panel-error-translating"
            );

            // Re-open the menu on an error.
            PanelMultiView.openPopup(panel, button, {
              position: "bottomright topright",
            }).catch(panelError => this.console.error(panelError));

            break;
          default:
            console.error("Unknown translation error", error);
        }
        break;
    }
  };
})();
