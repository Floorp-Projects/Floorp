// |reftest| skip-if(!this.hasOwnProperty("Intl")||!this.hasOwnProperty('addIntlExtras'))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the format function with a diverse set of locales and options.

var pr;

addIntlExtras(Intl);

pr = new Intl.PluralRules("en-us");
assertEq(pr.resolvedOptions().locale, "en-US");
assertEq(pr.resolvedOptions().type, "cardinal");
assertEq(pr.resolvedOptions().pluralCategories.length, 2);

pr = new Intl.PluralRules("de", {type: 'cardinal'});
assertEq(pr.resolvedOptions().pluralCategories.length, 2);

pr = new Intl.PluralRules("de", {type: 'ordinal'});
assertEq(pr.resolvedOptions().pluralCategories.length, 1);

reportCompare(0, 0, 'ok');
