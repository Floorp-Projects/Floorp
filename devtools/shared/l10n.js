/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const parsePropertiesFile = require("./node-properties/node-properties");
const { sprintf } = require("./sprintfjs/sprintf");

const propertiesMap = {};

// We need some special treatment here for webpack.
//
// Webpack doesn't always handle dynamic requires in the best way.  In
// particular if it sees an unrestricted dynamic require, it will try
// to put all the files it can find into the generated pack.  (It can
// also try a bit to parse the expression passed to require, but in
// our case this doesn't work, because our call below doesn't provide
// enough information.)
//
// Webpack also provides a way around this: require.context.  The idea
// here is to tell webpack some constraints so that it can include
// fewer files in the pack.
//
// Here we introduce new require contexts for each possible locale
// directory.  Then we use the correct context to load the property
// file.  In the webpack case this results in just the locale property
// files being included in the pack; and in the devtools case this is
// a wordy no-op.
const reqShared = require.context("raw!devtools/shared/locales/",
                                  true, /^.*\.properties$/);
const reqClient = require.context("raw!devtools/client/locales/",
                                  true, /^.*\.properties$/);
const reqStartup = require.context("raw!devtools/startup/locales/",
                                  true, /^.*\.properties$/);
const reqGlobal = require.context("raw!toolkit/locales/",
                                  true, /^.*\.properties$/);

// Map used to memoize Number formatters.
const numberFormatters = new Map();
const getNumberFormatter = function(decimals) {
  let formatter = numberFormatters.get(decimals);
  if (!formatter) {
    // Create and memoize a formatter for the provided decimals
    formatter = Intl.NumberFormat(undefined, {
      maximumFractionDigits: decimals,
      minimumFractionDigits: decimals
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
    // See the comment above about webpack and require contexts.  Here
    // we take an input like "devtools/shared/locales/debugger.properties"
    // and decide which context require function to use.  Despite the
    // string processing here, in the end a string identical to |url|
    // ends up being passed to "require".
    const index = url.lastIndexOf("/");
    // Turn "mumble/locales/resource.properties" => "./resource.properties".
    const baseName = "." + url.substr(index);
    let reqFn;
    if (/^toolkit/.test(url)) {
      reqFn = reqGlobal;
    } else if (/^devtools\/shared/.test(url)) {
      reqFn = reqShared;
    } else if (/^devtools\/startup/.test(url)) {
      reqFn = reqStartup;
    } else {
      reqFn = reqClient;
    }
    propertiesMap[url] = parsePropertiesFile(reqFn(baseName));
  }

  return propertiesMap[url];
}

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
  /**
   * L10N shortcut function.
   *
   * @param string name
   * @return string
   */
  getStr: function(name) {
    const properties = getProperties(this.stringBundleName);
    if (name in properties) {
      return properties[name];
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
  getFormatStr: function(name, ...args) {
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
  getFormatStrWithNumbers: function(name, ...args) {
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
  numberWithDecimals: function(number, decimals = 0) {
    // Do not show decimals for integers.
    if (number === (number|0)) {
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
    if (localizedNumber === (localizedNumber|0)) {
    // If it is, remove the fraction part.
      return getNumberFormatter(0).format(localizedNumber);
    }

    return localized;
  }
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

const sharedL10N = new LocalizationHelper("devtools/shared/locales/shared.properties");

/**
 * A helper for having the same interface as LocalizationHelper, but for more
 * than one file. Useful for abstracting l10n string locations.
 */
function MultiLocalizationHelper(...stringBundleNames) {
  const instances = stringBundleNames.map(bundle => {
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
Object.defineProperty(exports, "ELLIPSIS", { get: () => sharedL10N.getStr("ellipsis") });
