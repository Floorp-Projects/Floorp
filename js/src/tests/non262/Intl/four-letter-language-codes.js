// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Four letter language subtags are not allowed.
const languageTags = [
    "root", // Special meaning in Unicode CLDR locale identifiers.
    "Latn", // Unicode CLDR locale identifiers can start with a script subtag.
    "Flob", // And now some non-sense input.
    "ZORK",
    "Blah-latn",
    "QuuX-latn-us",
    "SPAM-gb-x-Sausages-BACON-eggs",
];

for (let tag of languageTags) {
    assertThrowsInstanceOf(() => Intl.getCanonicalLocales(tag), RangeError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
