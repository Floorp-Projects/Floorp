/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const { L10nRegistry } = require("resource://gre/modules/L10nRegistry.jsm");

class L10n {
  async init() {
    const locales = Services.locale.appLocalesAsBCP47;
    const generator = L10nRegistry.generateBundles(locales, [
      "branding/brand.ftl",
      "devtools/aboutdebugging.ftl",
    ]);

    this._bundles = [];
    for await (const bundle of generator) {
      this._bundles.push(bundle);
    }
    this._reactLocalization = new FluentReact.ReactLocalization(this._bundles);
  }

  /**
   * Returns the fluent bundles generated for about:debugging.
   */
  getBundles() {
    return this._bundles;
  }

  /**
   * Returns the localized string for the provided id, formatted using args.
   */
  getString(id, args, fallback) {
    // Forward arguments via .apply() so that the original method can:
    // - perform asserts based on the number of arguments
    // - add new arguments
    return this._reactLocalization.getString.apply(this._reactLocalization, arguments);
  }
}

// Export a singleton that will be shared by all aboutdebugging modules.
exports.l10n = new L10n();
