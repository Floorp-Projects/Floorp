/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global ExtensionAPI */

"use strict";

this.languageDetector = class extends ExtensionAPI {
  getAPI() {
    const { LanguageDetector } = ChromeUtils.import(
      "resource:///modules/translation/LanguageDetector.jsm",
      {},
    );

    const { ExtensionUtils } = ChromeUtils.import(
      "resource://gre/modules/ExtensionUtils.jsm",
      {},
    );
    const { ExtensionError } = ExtensionUtils;

    return {
      experiments: {
        languageDetector: {
          /* Detect language */
          detectLanguage: async function detectLanguage(str) {
            try {
              // console.log("Called detectLanguage(str)", str);
              return LanguageDetector.detectLanguage(str);
            } catch (error) {
              // Surface otherwise silent or obscurely reported errors
              console.error(error.message, error.stack);
              throw new ExtensionError(error.message);
            }
          },
        },
      },
    };
  }
};
