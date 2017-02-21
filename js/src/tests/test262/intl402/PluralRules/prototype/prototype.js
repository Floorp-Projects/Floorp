// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.hasOwnProperty('addIntlExtras')) -- needs Intl, needs addIntlExtras
// Copyright 2016 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: sec-properties-of-intl-pluralrules-prototype-object
description: >
    Tests that Intl.PluralRules.prototype is an object that  has been
    initialized as an Intl.PluralRules.
author: Zibi Braniecki
---*/

// test by calling a function that would fail if "this" were not an object
// initialized as an Intl.PluralRules
if (typeof Intl.PluralRules.prototype.select(0) !== "string") {
    $ERROR("Intl.PluralRules's prototype is not an object that has been " +
        "initialized as an Intl.PluralRules");
}

reportCompare(0, 0);
