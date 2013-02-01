/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci } = require("chrome");
const { getPreferedLocales, findClosestLocale } = require("./locale");
const { readURI } = require("../net/url");
const { resolve } = require("../core/promise");

function parseJsonURI(uri) {
  return readURI(uri).
    then(JSON.parse).
    then(null, function (error) {
      throw Error("Failed to parse locale file:\n" + uri + "\n" + error);
    });
}

// Returns the array stored in `locales.json` manifest that list available
// locales files
function getAvailableLocales(rootURI) {
  let uri = rootURI + "locales.json";
  return parseJsonURI(uri).then(function (manifest) {
    return "locales" in manifest &&
           Array.isArray(manifest.locales) ?
           manifest.locales : [];
  });
}

// Returns URI of the best locales file to use from the XPI
function getBestLocale(rootURI) {
  // Read localization manifest file that contains list of available languages
  return getAvailableLocales(rootURI).then(function (availableLocales) {
    // Retrieve list of prefered locales to use
    let preferedLocales = getPreferedLocales();

    // Compute the most preferable locale to use by using these two lists
    return findClosestLocale(availableLocales, preferedLocales);
  });
}

/**
 * Read localization files and returns a promise of data to put in `@l10n/data`
 * pseudo module, in order to allow l10n/core to fetch it.
 */
exports.load = function load(rootURI) {
  // First, search for a locale file:
  return getBestLocale(rootURI).then(function (bestMatchingLocale) {
    // It may be null if the addon doesn't have any locale file
    if (!bestMatchingLocale)
      return resolve(null);

    let localeURI = rootURI + "locale/" + bestMatchingLocale + ".json";

    // Locale files only contains one big JSON object that is used as
    // an hashtable of: "key to translate" => "translated key"
    // TODO: We are likely to change this in order to be able to overload
    //       a specific key translation. For a specific package, module or line?
    return parseJsonURI(localeURI).then(function (json) {
      return {
        hash: json,
        bestMatchingLocale: bestMatchingLocale
      };
    });
  });
}
