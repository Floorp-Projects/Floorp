/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  TranslationsParent: "resource://gre/actors/TranslationsParent.sys.mjs",
});

/**
 * A class containing static functionality that is shared by both
 * the FullPageTranslationsPanel and SelectTranslationsPanel classes.
 */
export class TranslationsPanelShared {
  /**
   * @type {Map<string, string>}
   */
  static #langListsInitState = new Map();

  /**
   * True if the next language-list initialization to fail for testing.
   *
   * @see TranslationsPanelShared.ensureLangListsBuilt
   *
   * @type {boolean}
   */
  static #simulateLangListError = false;

  /**
   * Clears cached data regarding the initialization state of the
   * FullPageTranslationsPanel or the SelectTranslationsPanel.
   *
   * This is only needed for test runners to ensure that each test
   * starts from a clean slate.
   */
  static clearCache() {
    this.#langListsInitState = new Map();
  }

  /**
   * Defines lazy getters for accessing elements in the document based on provided entries.
   *
   * @param {Document} document - The document object.
   * @param {object} lazyElements - An object where lazy getters will be defined.
   * @param {object} entries - An object of key/value pairs for which to define lazy getters.
   */
  static defineLazyElements(document, lazyElements, entries) {
    for (const [name, discriminator] of Object.entries(entries)) {
      let element;
      Object.defineProperty(lazyElements, name, {
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
            throw new Error(`Could not find "${name}" at "#${discriminator}".`);
          }
          return element;
        },
      });
    }
  }

  /**
   * Ensures that the next call to ensureLangListBuilt wil fail
   * for the purpose of testing the error state.
   *
   * @see TranslationsPanelShared.ensureLangListsBuilt
   *
   * @type {boolean}
   */
  static simulateLangListError() {
    this.#simulateLangListError = true;
  }

  /**
   * Retrieves the initialization state of language lists for the specified panel.
   *
   * @param {FullPageTranslationsPanel | SelectTranslationsPanel} panel
   *   - The panel for which to look up the state.
   * @param {number} innerWindowId - The id of the current window.
   */
  static getLangListsInitState(panel, innerWindowId) {
    const key = `${panel.id}-${innerWindowId}`;
    return TranslationsPanelShared.#langListsInitState.get(key);
  }

  /**
   * Builds the <menulist> of languages for both the "from" and "to". This can be
   * called every time the popup is shown, as it will retry when there is an error
   * (such as a network error) or be a noop if it's already initialized.
   *
   * @param {Document} document - The document object.
   * @param {FullPageTranslationsPanel | SelectTranslationsPanel} panel
   *   - The panel for which to ensure language lists are built.
   * @param {number} innerWindowId - The id of the current window.
   */
  static async ensureLangListsBuilt(document, panel, innerWindowId) {
    const key = `${panel.id}-${innerWindowId}`;
    switch (TranslationsPanelShared.#langListsInitState.get(key)) {
      case "initialized":
        // This has already been initialized.
        return;
      case "error":
      case undefined:
        // Set the error state in case there is an early exit at any point.
        // This will be set to "initialized" if everything succeeds.
        TranslationsPanelShared.#langListsInitState.set(key, "error");
        break;
      default:
        throw new Error(
          `Unknown langList phase ${
            TranslationsPanelShared.#langListsInitState
          }`
        );
    }
    /** @type {SupportedLanguages} */
    const { languagePairs, fromLanguages, toLanguages } =
      await lazy.TranslationsParent.getSupportedLanguages();

    // Verify that we are in a proper state.
    if (languagePairs.length === 0 || this.#simulateLangListError) {
      this.#simulateLangListError = false;
      throw new Error("No translation languages were retrieved.");
    }

    const fromPopups = panel.querySelectorAll(
      ".translations-panel-language-menupopup-from"
    );
    const toPopups = panel.querySelectorAll(
      ".translations-panel-language-menupopup-to"
    );

    for (const popup of fromPopups) {
      // For the moment, the FullPageTranslationsPanel includes its own
      // menu item for "Choose another language" as the first item in the list
      // with an empty-string for its value. The SelectTranslationsPanel has
      // only languages in its list with BCP-47 tags for values. As such,
      // this loop works for both panels, to remove all of the languages
      // from the list, but ensuring that any empty-string items are retained.
      while (popup.lastChild?.value) {
        popup.lastChild.remove();
      }
      for (const { langTag, displayName } of fromLanguages) {
        const fromMenuItem = document.createXULElement("menuitem");
        fromMenuItem.setAttribute("value", langTag);
        fromMenuItem.setAttribute("label", displayName);
        popup.appendChild(fromMenuItem);
      }
    }

    for (const popup of toPopups) {
      while (popup.lastChild?.value) {
        popup.lastChild.remove();
      }
      for (const { langTag, displayName } of toLanguages) {
        const toMenuItem = document.createXULElement("menuitem");
        toMenuItem.setAttribute("value", langTag);
        toMenuItem.setAttribute("label", displayName);
        popup.appendChild(toMenuItem);
      }
    }

    TranslationsPanelShared.#langListsInitState.set(key, "initialized");
  }
}
