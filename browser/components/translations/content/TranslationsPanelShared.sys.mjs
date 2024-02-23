/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A class containing static functionality that is shared by both
 * the FullPageTranslationsPanel and SelectTranslationsPanel classes.
 */
export class TranslationsPanelShared {
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
}
