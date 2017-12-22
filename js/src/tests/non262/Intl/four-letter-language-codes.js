// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// So many non-existent four letter language codes to pick from.
const languageTags = {
    "Flob": "flob",
    "ZORK": "zork",
    "Blah-latn": "blah-Latn",
    "QuuX-latn-us": "quux-Latn-US",
    "SPAM-gb-x-Sausages-BACON-eggs": "spam-GB-x-sausages-bacon-eggs",
};

for (let [tag, canonical] of Object.entries(languageTags)) {
    assertEq(Intl.getCanonicalLocales(tag)[0], canonical);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
