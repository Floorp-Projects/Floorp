// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.hasOwnProperty('addIntlExtras')) -- needs Intl, needs addIntlExtras
// Copyright 2016 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: sec-properties-of-intl-pluralrules-prototype-object
description: >
    Tests that Intl.PluralRules.prototype functions throw a TypeError if
    called on a non-object value or an object that hasn't been
    initialized as a PluralRules.
author: Zibi Braniecki
---*/

var functions = {
    select: Intl.PluralRules.prototype.select,
    resolvedOptions: Intl.PluralRules.prototype.resolvedOptions
};
var invalidTargets = [undefined, null, true, 0, "PluralRules", [], {}];

Object.getOwnPropertyNames(functions).forEach(function (functionName) {
    var f = functions[functionName];
    invalidTargets.forEach(function (target) {
        var error;
        try {
            f.call(target);
        } catch (e) {
            error = e;
        }
        if (error === undefined) {
            $ERROR("Calling " + functionName + " on " + target + " was not rejected.");
        } else if (error.name !== "TypeError") {
            $ERROR("Calling " + functionName + " on " + target + " was rejected with wrong error " + error.name + ".");
        }
    });
});

reportCompare(0, 0);
