/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const { L10nRegistry } = require("resource://gre/modules/L10nRegistry.jsm");

/**
 * Wrapper over FluentReact. It encapsulates instantiation of the localization
 * bundles, and offers a simpler way of accessing `getString`.
 */
class FluentL10n {
  /**
   * Initializes the wrapper, generating the bundles for the given resource ids.
   * @param {Array} resourceIds
   */
  async init(resourceIds) {
    const locales = Services.locale.appLocalesAsBCP47;
    const generator = L10nRegistry.generateBundles(locales, resourceIds);

    this._bundles = [];
    for await (const bundle of generator) {
      this._bundles.push(bundle);
    }
    this._reactLocalization = new FluentReact.ReactLocalization(this._bundles);
  }

  /**
   * Returns the fluent bundles generated.
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
    return this._reactLocalization.getString.apply(
      this._reactLocalization,
      arguments
    );
  }
}

// Export the class
exports.FluentL10n = FluentL10n;
