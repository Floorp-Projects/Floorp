// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the getCanonicalLocales function for duplicate locales scenario.

assertEqArray(Intl.getCanonicalLocales(['ab-cd', 'ff', 'de-rt', 'ab-Cd']),
              ['ab-CD', 'ff', 'de-RT']);

var locales = Intl.getCanonicalLocales(["en-US", "en-US"]);
assertEqArray(locales, ['en-US']);

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
