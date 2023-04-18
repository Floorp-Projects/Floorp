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

      // Lazily select the elements.
      this.#lazyElements = {
        panel,

        get button() {
          delete this.button;
          return (this.button = document.getElementById("translations-button"));
        },
        get fromMenuPopup() {
          delete this.fromMenuPopup;
          return (this.fromMenuPopup = document.getElementById(
            "translations-panel-from-menupopup"
          ));
        },
        get toMenuPopup() {
          delete this.toMenuPopup;
          return (this.toMenuPopup = document.getElementById(
            "translations-panel-to-menupopup"
          ));
        },
        get multiview() {
          delete this.multiview;
          return (this.multiview = document.getElementById(
            "translations-panel-multiview"
          ));
        },
        get dualView() {
          delete this.dualView;
          return (this.dualView = document.getElementById(
            "translations-panel-view-dual"
          ));
        },
        get restoreView() {
          delete this.restoreView;
          return (this.restoreView = document.getElementById(
            "translations-panel-view-restore"
          ));
        },
        get fromMenuList() {
          delete this.fromMenuList;
          return (this.fromMenuList = document.getElementById(
            "translations-panel-from"
          ));
        },
        get toMenuList() {
          delete this.toMenuList;
          return (this.toMenuList = document.getElementById(
            "translations-panel-to"
          ));
        },
        get restoreLabel() {
          delete this.restoreLabel;
          return (this.restoreLabel = document.getElementById(
            "translations-panel-restore-label"
          ));
        },
      };
    }

    return this.#lazyElements;
  }

  /**
   * @returns {TranslationsParent}
   */
  #getTranslationsActor() {
    const actor = gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getActor(
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
      const {
        languagePairs,
        fromLanguages,
        toLanguages,
      } = await this.#getTranslationsActor().getSupportedLanguages();

      // Verify that we are in a proper state.
      if (languagePairs.length === 0) {
        throw new Error("No translation languages were retrieved.");
      }

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
        this.elements.fromMenuPopup.appendChild(fromMenuItem);
      }
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
        this.elements.toMenuPopup.appendChild(toMenuItem);
      }
      this.#langListsPhase = "initialized";
    } catch (error) {
      this.console.error(error);
      this.#langListsPhase = "error";
    }
  }

  /**
   * Builds the <menulist> of languages for both the "from" and "to". This can be
   * called every time the popup is shown, as it will retry when there is an error
   * (such as a network error) or be a noop if it's already initialized.
   *
   * @param {Promise<void>} langListBuilt
   */
  async #setDualView(langListBuilt) {
    const actor = this.#getTranslationsActor();

    const { fromMenuList, toMenuList, multiview } = this.elements;

    multiview.setAttribute("mainViewId", "translations-panel-view-dual");

    // Remove any old selected values synchronously before asking for new ones.
    fromMenuList.value = "";
    toMenuList.value = "";

    // TODO(Bug 1825801) - There is a race condition, we may download the languages, and
    // later trigger the subview to be shown after opening the popup again. We need to
    // properly handle this.

    // TODO(Bug 1825801) - This could potentially be a bad pause, as we aren't showing
    // the panel until the language list is ready. It's probably fine for a prototype,
    // but should be handled for the MVP. We might want design direction here, as we need
    // a subview for when the language list is still being retrieved.

    /** @type {null | { appLangTag: string, docLangTag: string }} */
    const langTags = await actor.getLangTagsForTranslation();
    await langListBuilt;

    if (langTags) {
      const { docLangTag, appLangTag } = langTags;
      fromMenuList.value = docLangTag;
      toMenuList.value = appLangTag;
    } else {
      this.console.error("No language tags for translation were found.");
    }
  }

  /**
   * Configures the panel for the user to reset the page after it has been translated.
   *
   * @param {TranslationPair} translationPair
   */
  #setRestoreView({ fromLanguage, toLanguage }) {
    const { multiview, restoreLabel } = this.elements;

    multiview.setAttribute("mainViewId", "translations-panel-view-restore");

    const displayNames = new Services.intl.DisplayNames(undefined, {
      type: "language",
    });

    restoreLabel.setAttribute(
      "data-l10n-args",
      JSON.stringify({
        fromLanguage: displayNames.of(fromLanguage),
        toLanguage: displayNames.of(toLanguage),
      })
    );
  }

  /**
   * Opens the TranslationsPanel.
   *
   * @param {Event} event
   */
  open(event) {
    const { panel, button } = this.elements;

    const {
      requestedTranslationPair,
    } = this.#getTranslationsActor().languageState;

    if (requestedTranslationPair) {
      this.#setRestoreView(requestedTranslationPair);
    } else {
      this.#setDualView(this.#ensureLangListsBuilt()).catch(error => {
        this.console.error(error);
      });
    }

    PanelMultiView.openPopup(panel, button, {
      position: "bottomright topright",
      triggerEvent: event,
    }).catch(error => this.console.error(error));
  }

  /**
   * Handle the translation button being clicked.
   */
  async onTranslate() {
    PanelMultiView.hidePopup(this.elements.panel);

    const actor = this.#getTranslationsActor();
    actor.translate(
      document.getElementById("translations-panel-from").value,
      document.getElementById("translations-panel-to").value
    );
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
        const { detectedLanguages, requestedTranslationPair } = event.detail;
        const { button } = this.elements;

        if (detectedLanguages) {
          button.hidden = false;
          if (requestedTranslationPair) {
            button.setAttribute("translationsactive", true);
          } else {
            button.removeAttribute("translationsactive");
          }
        } else {
          button.removeAttribute("translationsactive");
          button.hidden = true;
        }
        break;
    }
  };
})();
