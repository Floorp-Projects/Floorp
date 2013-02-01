/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { getRulesForLocale } = require("sdk/l10n/plural-rules");

// For more information, please visit unicode website:
// http://unicode.org/repos/cldr-tmp/trunk/diff/supplemental/language_plural_rules.html

function map(test, f, n, form) {
  test.assertEqual(f(n), form, n + " maps to '" + form + "'");
}

exports.testFrench = function(test) {
  let f = getRulesForLocale("fr");
  map(test, f, -1, "other");
  map(test, f, 0, "one");
  map(test, f, 1, "one");
  map(test, f, 1.5, "one");
  map(test, f, 2, "other");
  map(test, f, 100, "other");
}

exports.testEnglish = function(test) {
  let f = getRulesForLocale("en");
  map(test, f, -1, "other");
  map(test, f, 0, "other");
  map(test, f, 1, "one");
  map(test, f, 1.5, "other");
  map(test, f, 2, "other");
  map(test, f, 100, "other");
}

exports.testArabic = function(test) {
  let f = getRulesForLocale("ar");
  map(test, f, -1, "other");
  map(test, f, 0, "zero");
  map(test, f, 0.5, "other");

  map(test, f, 1, "one");
  map(test, f, 1.5, "other");

  map(test, f, 2, "two");
  map(test, f, 2.5, "other");

  map(test, f, 3, "few");
  map(test, f, 3.5, "few"); // I'd expect it to be 'other', but the unicode.org
                            // algorithm computes 'few'.
  map(test, f, 5, "few");
  map(test, f, 10, "few");
  map(test, f, 103, "few");
  map(test, f, 105, "few");
  map(test, f, 110, "few");
  map(test, f, 203, "few");
  map(test, f, 205, "few");
  map(test, f, 210, "few");

  map(test, f, 11, "many");
  map(test, f, 50, "many");
  map(test, f, 99, "many");
  map(test, f, 111, "many");
  map(test, f, 150, "many");
  map(test, f, 199, "many");

  map(test, f, 100, "other");
  map(test, f, 101, "other");
  map(test, f, 102, "other");
  map(test, f, 200, "other");
  map(test, f, 201, "other");
  map(test, f, 202, "other");
}

exports.testJapanese = function(test) {
  // Japanese doesn't have plural forms.
  let f = getRulesForLocale("ja");
  map(test, f, -1, "other");
  map(test, f, 0, "other");
  map(test, f, 1, "other");
  map(test, f, 1.5, "other");
  map(test, f, 2, "other");
  map(test, f, 100, "other");
}
