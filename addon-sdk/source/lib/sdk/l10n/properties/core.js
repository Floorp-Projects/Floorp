/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");
const { newURI } = require('../../url/utils')
const { getRulesForLocale } = require("../plural-rules");
const { getPreferedLocales } = require('../locale');
const { rootURI } = require("@loader/options");
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

const baseURI = rootURI + "locale/";
const preferedLocales = getPreferedLocales(true);

function getLocaleURL(locale) {
  // if the locale is a valid chrome URI, return it
  try {
    let uri = newURI(locale);
    if (uri.scheme == 'chrome')
      return uri.spec;
  }
  catch(_) {}
  // otherwise try to construct the url
  return baseURI + locale + ".properties";
}

function getKey(locale, key) {
  let bundle = Services.strings.createBundle(getLocaleURL(locale));
  try {
    return bundle.GetStringFromName(key) + "";
  }
  catch (_) {}
  return undefined;
}

function get(key, n, locales) {
  // try this locale
  let locale = locales.shift();
  let localized;

  if (typeof n == 'number') {
    if (n == 0) {
      localized = getKey(locale, key + '[zero]');
    }
    else if (n == 1) {
      localized = getKey(locale, key + '[one]');
    }
    else if (n == 2) {
      localized = getKey(locale, key + '[two]');
    }

    if (!localized) {
      // Retrieve the plural mapping function
      let pluralForm = (getRulesForLocale(locale.split("-")[0].toLowerCase()) ||
                        getRulesForLocale("en"))(n);
      localized = getKey(locale, key + '[' + pluralForm + ']');
    }

    if (!localized) {
      localized = getKey(locale, key + '[other]');
    }
  }

  if (!localized) {
    localized = getKey(locale, key);
  }

  if (!localized) {
    localized = getKey(locale, key + '[other]');
  }

  if (localized) {
    return localized;
  }

  // try next locale
  if (locales.length)
    return get(key, n, locales);

  return undefined;
}
exports.get = (k, n) => get(k, n, Array.slice(preferedLocales));
