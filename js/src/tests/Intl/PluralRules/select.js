// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.hasOwnProperty('addIntlExtras'))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the format function with a diverse set of locales and options.

var pr;

addIntlExtras(Intl);

pr = new Intl.PluralRules("en-us");
assertEq(pr.select(0), "other");
assertEq(pr.select(0.5), "other");
assertEq(pr.select(1.2), "other");
assertEq(pr.select(1.5), "other");
assertEq(pr.select(1.7), "other");
assertEq(pr.select(-1), "one");
assertEq(pr.select(1), "one");
assertEq(pr.select("1"), "one");
assertEq(pr.select(123456789.123456789), "other");

pr = new Intl.PluralRules("de", {type: "cardinal"});
assertEq(pr.select(0), "other");
assertEq(pr.select(0.5), "other");
assertEq(pr.select(1.2), "other");
assertEq(pr.select(1.5), "other");
assertEq(pr.select(1.7), "other");
assertEq(pr.select(-1), "one");

pr = new Intl.PluralRules("de", {type: "ordinal"});
assertEq(pr.select(0), "other");
assertEq(pr.select(0.5), "other");
assertEq(pr.select(1.2), "other");
assertEq(pr.select(1.5), "other");
assertEq(pr.select(1.7), "other");
assertEq(pr.select(-1), "other");

pr = new Intl.PluralRules("pl", {type: "cardinal"});
assertEq(pr.select(0), "many");
assertEq(pr.select(0.5), "other");
assertEq(pr.select(1), "one");

pr = new Intl.PluralRules("pl", {type: "cardinal", maximumFractionDigits: 0});
assertEq(pr.select(1.1), "one");

pr = new Intl.PluralRules("pl", {type: "cardinal", maximumFractionDigits: 1});
assertEq(pr.select(1.1), "other");

var weirdCases = [
  NaN,
  Infinity,
  "word",
  [0,2],
  {},
];

for (let c of weirdCases) {
  assertEq(pr.select(c), "other");
};

reportCompare(0, 0, 'ok');
