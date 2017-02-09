// |reftest| skip-if(!this.hasOwnProperty('Intl')) -- needs Intl
// Copyright 2016 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: sec-Intl.PluralRules.prototype.constructor
description: >
    Tests that Intl.PluralRules.prototype is an object that has been
    initialized as an Intl.PluralRules.
author: Zibi Braniecki
---*/

if (Intl.PluralRules.prototype.constructor !== Intl.PluralRules) {
    $ERROR("Intl.PluralRules.prototype.constructor is not the same as " +
          "Intl.PluralRules");
}

reportCompare(0, 0);
