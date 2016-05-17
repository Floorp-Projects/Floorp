/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci } = require("chrome");
const Services = require("Services");

/**
 * Localization convenience methods.
 *
 * @param string stringBundleName
 *        The desired string bundle's name.
 */
function LocalizationHelper(stringBundleName) {
  loader.lazyGetter(this, "stringBundle", () =>
    Services.strings.createBundle(stringBundleName));
  loader.lazyGetter(this, "ellipsis", () =>
    Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString).data);
}

LocalizationHelper.prototype = {
  /**
   * L10N shortcut function.
   *
   * @param string name
   * @return string
   */
  getStr: function (name) {
    return this.stringBundle.GetStringFromName(name);
  },

  /**
   * L10N shortcut function.
   *
   * @param string name
   * @param array args
   * @return string
   */
  getFormatStr: function (name, ...args) {
    return this.stringBundle.formatStringFromName(name, args, args.length);
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
    let newArgs = args.map(x => typeof x == "number" ? this.numberWithDecimals(x, 2) : x);
    return this.stringBundle.formatStringFromName(name, newArgs, newArgs.length);
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

/**
 * A helper for having the same interface as LocalizationHelper, but for more than
 * one file. Useful for abstracting l10n string locations.
 */
function MultiLocalizationHelper(...stringBundleNames) {
  let instances = stringBundleNames.map(bundle => new LocalizationHelper(bundle));

  // Get all function members of the LocalizationHelper class, making sure we're not
  // executing any potential getters while doing so, and wrap all the
  // methods we've found to work on all given string bundles.
  Object.getOwnPropertyNames(LocalizationHelper.prototype)
    .map(name => ({
      name: name,
      descriptor: Object.getOwnPropertyDescriptor(LocalizationHelper.prototype, name)
    }))
    .filter(({ descriptor }) => descriptor.value instanceof Function)
    .forEach(method => {
      this[method.name] = (...args) => {
        for (let l10n of instances) {
          try {
            return method.descriptor.value.apply(l10n, args);
          } catch (e) {}
        }
      };
    });
}

exports.LocalizationHelper = LocalizationHelper;
exports.MultiLocalizationHelper = MultiLocalizationHelper;
