/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const parsePropertiesFile = require("devtools/shared/node-properties/node-properties");
const { sprintf } = require("devtools/shared/sprintfjs/sprintf");

const propertiesMap = {};

// Map used to memoize Number formatters.
const numberFormatters = new Map();
const getNumberFormatter = function(decimals) {
  let formatter = numberFormatters.get(decimals);
  if (!formatter) {
    // Create and memoize a formatter for the provided decimals
    formatter = Intl.NumberFormat(undefined, {
      maximumFractionDigits: decimals,
      minimumFractionDigits: decimals,
    });
    numberFormatters.set(decimals, formatter);
  }

  return formatter;
};

/**
 * Memoized getter for properties files that ensures a given url is only required and
 * parsed once.
 *
 * @param {String} url
 *        The URL of the properties file to parse.
 * @return {Object} parsed properties mapped in an object.
 */
function getProperties(url) {
  if (!propertiesMap[url]) {
    let propertiesFile;
    let isNodeEnv = false;
    try {
      // eslint-disable-next-line no-undef
      isNodeEnv = process?.release?.name == "node";
    } catch (e) {}

    if (isNodeEnv) {
      // In Node environment (e.g. when running jest test), we need to prepend the en-US
      // to the filename in order to have the actual location of the file in source.
      const lastDelimIndex = url.lastIndexOf("/");
      const defaultLocaleUrl =
        url.substring(0, lastDelimIndex) +
        "/en-US" +
        url.substring(lastDelimIndex);

      const path = require("path");
      // eslint-disable-next-line no-undef
      const rootPath = path.join(__dirname, "../../");
      const absoluteUrl = path.join(rootPath, defaultLocaleUrl);
      const { readFileSync } = require("fs");
      // In Node environment we directly use readFileSync to get the file content instead
      // of relying on custom raw loader, like we do in regular environment.
      propertiesFile = readFileSync(absoluteUrl, { encoding: "utf8" });
    } else {
      propertiesFile = require("raw!" + url);
    }

    propertiesMap[url] = parsePropertiesFile(propertiesFile);
  }

  return propertiesMap[url];
}

/**
 * Localization convenience methods.
 *
 * @param string stringBundleName
 *        The desired string bundle's name.
 * @param boolean strict
 *        (legacy) pass true to force the helper to throw if the l10n id cannot be found.
 */
function LocalizationHelper(stringBundleName, strict = false) {
  this.stringBundleName = stringBundleName;
  this.strict = strict;
}

LocalizationHelper.prototype = {
  /**
   * L10N shortcut function.
   *
   * @param string name
   * @return string
   */
  getStr(name) {
    const properties = getProperties(this.stringBundleName);
    if (name in properties) {
      return properties[name];
    }

    if (this.strict) {
      throw new Error("No localization found for [" + name + "]");
    }

    console.error("No localization found for [" + name + "]");
    return name;
  },

  /**
   * L10N shortcut function.
   *
   * @param string name
   * @param array args
   * @return string
   */
  getFormatStr(name, ...args) {
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
  getFormatStrWithNumbers(name, ...args) {
    const newArgs = args.map(x => {
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
  numberWithDecimals(number, decimals = 0) {
    // Do not show decimals for integers.
    if (number === (number | 0)) {
      return getNumberFormatter(0).format(number);
    }

    // If this isn't a number (and yes, `isNaN(null)` is false), return zero.
    if (isNaN(number) || number === null) {
      return getNumberFormatter(0).format(0);
    }

    // Localize the number using a memoized Intl.NumberFormat formatter.
    const localized = getNumberFormatter(decimals).format(number);

    // Convert the localized number to a number again.
    const localizedNumber = localized * 1;
    // Check if this number is now equal to an integer.
    if (localizedNumber === (localizedNumber | 0)) {
      // If it is, remove the fraction part.
      return getNumberFormatter(0).format(localizedNumber);
    }

    return localized;
  },
};

function getPropertiesForNode(node) {
  const bundleEl = node.closest("[data-localization-bundle]");
  if (!bundleEl) {
    return null;
  }

  const propertiesUrl = bundleEl.getAttribute("data-localization-bundle");
  return getProperties(propertiesUrl);
}

/**
 * Translate existing markup annotated with data-localization attributes.
 *
 * How to use data-localization in markup:
 *
 *   <div data-localization="content=myContent;title=myTitle"/>
 *
 * The data-localization attribute identifies an element as being localizable.
 * The content of the attribute is semi-colon separated list of descriptors.
 * - "title=myTitle" means the "title" attribute should be replaced with the localized
 *   string corresponding to the key "myTitle".
 * - "content=myContent" means the text content of the node should be replaced by the
 *   string corresponding to "myContent"
 *
 * How to define the localization bundle in markup:
 *
 *   <div data-localization-bundle="url/to/my.properties">
 *     [...]
 *       <div data-localization="content=myContent;title=myTitle"/>
 *
 * Set the data-localization-bundle on an ancestor of the nodes that should be localized.
 *
 * @param {Element} root
 *        The root node to use for the localization
 */
function localizeMarkup(root) {
  const elements = root.querySelectorAll("[data-localization]");
  for (const element of elements) {
    const properties = getPropertiesForNode(element);
    if (!properties) {
      continue;
    }

    const attributes = element.getAttribute("data-localization").split(";");
    for (const attribute of attributes) {
      const [name, value] = attribute.trim().split("=");
      if (name === "content") {
        element.textContent = properties[value];
      } else {
        element.setAttribute(name, properties[value]);
      }
    }

    element.removeAttribute("data-localization");
  }
}

const sharedL10N = new LocalizationHelper(
  "devtools/shared/locales/shared.properties"
);

/**
 * A helper for having the same interface as LocalizationHelper, but for more
 * than one file. Useful for abstracting l10n string locations.
 */
function MultiLocalizationHelper(...stringBundleNames) {
  const instances = stringBundleNames.map(bundle => {
    // Use strict = true because the MultiLocalizationHelper logic relies on try/catch
    // around the underlying LocalizationHelper APIs.
    return new LocalizationHelper(bundle, true);
  });

  // Get all function members of the LocalizationHelper class, making sure we're
  // not executing any potential getters while doing so, and wrap all the
  // methods we've found to work on all given string bundles.
  Object.getOwnPropertyNames(LocalizationHelper.prototype)
    .map(name => ({
      name,
      descriptor: Object.getOwnPropertyDescriptor(
        LocalizationHelper.prototype,
        name
      ),
    }))
    .filter(({ descriptor }) => descriptor.value instanceof Function)
    .forEach(method => {
      this[method.name] = (...args) => {
        for (const l10n of instances) {
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
exports.localizeMarkup = localizeMarkup;
exports.MultiLocalizationHelper = MultiLocalizationHelper;
Object.defineProperty(exports, "ELLIPSIS", {
  get: () => sharedL10N.getStr("ellipsis"),
});
