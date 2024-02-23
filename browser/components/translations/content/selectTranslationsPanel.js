/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

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
      getter("betaIcon", "select-translations-panel-beta-icon");
      getter("copyButton", "select-translations-panel-copy-button");
      getter("doneButton", "select-translations-panel-done-button");
      getter("fromLabel", "select-translations-panel-from-label");
      getter("fromMenuList", "select-translations-panel-from");
      getter("header", "select-translations-panel-header");
      getter("multiview", "select-translations-panel-multiview");
      getter("textArea", "select-translations-panel-translation-area");
      getter("toLabel", "select-translations-panel-to-label");
      getter("toMenuList", "select-translations-panel-to");
      getter(
        "translateFullPageButton",
        "select-translations-panel-translate-full-page-button"
      );
    }

    return this.#lazyElements;
  }

  /**
   * Close the Select Translations Panel.
   */
  close() {
    PanelMultiView.hidePopup(this.elements.panel);
  }

  /**
   * Open the Select Translations Panel.
   */
  async open(event) {
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
})();
