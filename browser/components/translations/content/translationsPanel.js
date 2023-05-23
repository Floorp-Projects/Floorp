/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

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
   * The automatically determined document lang tag.
   *
   * @type {null | string}
   */
  #docLangTag = null;

  /**
   * Keep track if the panel has been shown yet this session.
   */
  #wasPanelShown = false;

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

    /** @type {null | { appLangTag: string, docLangTag: string }} */
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

    PanelMultiView.openPopup(panel, button, {
      position: "bottomright topright",
      triggerEvent: event,
    }).catch(error => this.console.error(error));
  }

  /**
   * Handle the translation button being clicked on the default view.
   */
  async onDefaultTranslate() {
    PanelMultiView.hidePopup(this.elements.panel);

    const actor = this.#getTranslationsActor();
    actor.translate(this.#docLangTag, this.elements.defaultToMenuList.value);
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
    actor.translate(this.#docLangTag, this.elements.revisitMenuList.value);
  }

  onCancel() {
    PanelMultiView.hidePopup(this.elements.panel);
  }

  /**
   * A handler for opening the settings context menu.
   */
  openSettingsPopup(button) {
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
   * Handle the restore button being clicked.
   */
  onRestore() {
    const { panel } = this.elements;
    PanelMultiView.hidePopup(panel);

    this.#getTranslationsActor().restorePage();
  }

  /**
   * Set the state of the translations button in the URL bar.
   *
   * @param {CustomEvent} event
   */
  handleEvent = event => {
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

        if (detectedLanguages) {
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
          button.removeAttribute("translationsactive");
          button.hidden = true;
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
