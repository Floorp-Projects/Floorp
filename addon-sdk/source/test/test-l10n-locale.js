/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu, Cc, Ci } = require("chrome");
const { Services } = Cu.import('resource://gre/modules/Services.jsm');
const { findClosestLocale,
        getPreferedLocales } = require("sdk/l10n/locale");

exports.testFindClosestLocale = function(assert) {
  // Second param of findClosestLocale (aMatchLocales) have to be in lowercase
  assert.equal(findClosestLocale([], []), null,
                   "When everything is empty we get null");

  assert.equal(findClosestLocale(["en", "en-US"], ["en"]),
                   "en", "We always accept exact match first 1/5");
  assert.equal(findClosestLocale(["en-US", "en"], ["en"]),
                   "en", "We always accept exact match first 2/5");
  assert.equal(findClosestLocale(["en", "en-US"], ["en-us"]),
                   "en-US", "We always accept exact match first 3/5");
  assert.equal(findClosestLocale(["ja-JP-mac", "ja", "ja-JP"], ["ja-jp"]),
                   "ja-JP", "We always accept exact match first 4/5");
  assert.equal(findClosestLocale(["ja-JP-mac", "ja", "ja-JP"], ["ja-jp-mac"]),
                   "ja-JP-mac", "We always accept exact match first 5/5");

  assert.equal(findClosestLocale(["en", "en-GB"], ["en-us"]),
                   "en", "We accept more generic locale, when there is no exact match 1/2");
  assert.equal(findClosestLocale(["en-ZA", "en"], ["en-gb"]),
                   "en", "We accept more generic locale, when there is no exact match 2/2");

  assert.equal(findClosestLocale(["ja-JP"], ["ja"]),
                   "ja-JP", "We accept more specialized locale, when there is no exact match 1/2");
  // Better to select "ja" in this case but behave same as current AddonManager
  assert.equal(findClosestLocale(["ja-JP-mac", "ja"], ["ja-jp"]),
                   "ja-JP-mac", "We accept more specialized locale, when there is no exact match 2/2");

  assert.equal(findClosestLocale(["en-US"], ["en-us"]),
                   "en-US", "We keep the original one as result 1/2");
  assert.equal(findClosestLocale(["en-us"], ["en-us"]),
                   "en-us", "We keep the original one as result 2/2");

  assert.equal(findClosestLocale(["ja-JP-mac"], ["ja-jp-mac"]),
                   "ja-JP-mac", "We accept locale with 3 parts");
  assert.equal(findClosestLocale(["ja-JP"], ["ja-jp-mac"]),
                   "ja-JP", "We accept locale with 2 parts from locale with 3 parts");
  assert.equal(findClosestLocale(["ja"], ["ja-jp-mac"]),
                   "ja", "We accept locale with 1 part from locale with 3 parts");
};

exports.testGetPreferedLocales = function(assert) {
  let currentLocales = Services.locale.getRequestedLocales();

  Services.locale.setRequestedLocales(['pl']);

  assert.equal(getPreferedLocales().includes('en-US'), true, "en-US should always be in the list");

  Services.locale.setRequestedLocales(currentLocales);
};

require('sdk/test').run(exports);
