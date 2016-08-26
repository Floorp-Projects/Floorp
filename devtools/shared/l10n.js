/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const parsePropertiesFile = require("devtools/shared/node-properties/node-properties");
const { sprintf } = require("devtools/shared/sprintfjs/sprintf");

/**
 * Localization convenience methods.
 *
 * @param string stringBundleName
 *        The desired string bundle's name.
 */
function LocalizationHelper(stringBundleName) {
  this.stringBundleName = stringBundleName;
}

LocalizationHelper.prototype = {
  get properties() {
    if (!this._properties) {
      this._properties = parsePropertiesFile(require(`raw!${this.stringBundleName}`));
    }

    return this._properties;
  },

  /**
   * L10N shortcut function.
   *
   * @param string name
   * @return string
   */
  getStr: function (name) {
    if (name in this.properties) {
      return this.properties[name];
    }

    throw new Error("No localization found for [" + name + "]");
  },

  /**
   * L10N shortcut function.
   *
   * @param string name
   * @param array args
   * @return string
   */
  getFormatStr: function (name, ...args) {
    return sprintf(this.getStr(name), ...args);
  },

  /**
   * L10N shortcut function for numeric arguments that need to be formatted.
   * All numeric arguments will be fixed to 2 decimals and given a localized
   * decimal separator. Other arguments will be left alone.
   *
   * @param string name
   * @param array args
   * @return string
   */
  getFormatStrWithNumbers: function (name, ...args) {
    let newArgs = args.map(x => {
      return typeof x == "number" ? this.numberWithDecimals(x, 2) : x;
    });

    return this.getFormatStr(name, ...newArgs);
  },

  /**
   * Converts a number to a locale-aware string format and keeps a certain
   * number of decimals.
   *
   * @param number number
   *        The number to convert.
   * @param number decimals [optional]
   *        Total decimals to keep.
   * @return string
   *         The localized number as a string.
   */
  numberWithDecimals: function (number, decimals = 0) {
    // If this is an integer, don't do anything special.
    if (number === (number|0)) {
      return number;
    }
    // If this isn't a number (and yes, `isNaN(null)` is false), return zero.
    if (isNaN(number) || number === null) {
      return "0";
    }

    let localized = number.toLocaleString();

    // If no grouping or decimal separators are available, bail out, because
    // padding with zeros at the end of the string won't make sense anymore.
    if (!localized.match(/[^\d]/)) {
      return localized;
    }

    return number.toLocaleString(undefined, {
      maximumFractionDigits: decimals,
      minimumFractionDigits: decimals
    });
  }
};

const sharedL10N = new LocalizationHelper("devtools-shared/locale/shared.properties");
const ELLIPSIS = sharedL10N.getStr("ellipsis");

/**
 * A helper for having the same interface as LocalizationHelper, but for more
 * than one file. Useful for abstracting l10n string locations.
 */
function MultiLocalizationHelper(...stringBundleNames) {
  let instances = stringBundleNames.map(bundle => {
    return new LocalizationHelper(bundle);
  });

  // Get all function members of the LocalizationHelper class, making sure we're
  // not executing any potential getters while doing so, and wrap all the
  // methods we've found to work on all given string bundles.
  Object.getOwnPropertyNames(LocalizationHelper.prototype)
    .map(name => ({
      name: name,
      descriptor: Object.getOwnPropertyDescriptor(LocalizationHelper.prototype,
                                                  name)
    }))
    .filter(({ descriptor }) => descriptor.value instanceof Function)
    .forEach(method => {
      this[method.name] = (...args) => {
        for (let l10n of instances) {
          try {
            return method.descriptor.value.apply(l10n, args);
          } catch (e) {
            // Do nothing
          }
        }
        return null;
      };
    });
}

exports.LocalizationHelper = LocalizationHelper;
exports.MultiLocalizationHelper = MultiLocalizationHelper;
exports.ELLIPSIS = ELLIPSIS;
