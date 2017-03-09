// Copyright 2016 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: sec-properties-of-intl-pluralrules-prototype-object
description: Tests that Intl.PluralRules.prototype has the required attributes.
author: Zibi Braniecki
---*/

var desc = Object.getOwnPropertyDescriptor(Intl.PluralRules, "prototype");
if (desc === undefined) {
    $ERROR("Intl.PluralRules.prototype is not defined.");
}
if (desc.writable) {
    $ERROR("Intl.PluralRules.prototype must not be writable.");
}
if (desc.enumerable) {
    $ERROR("Intl.PluralRules.prototype must not be enumerable.");
}
if (desc.configurable) {
    $ERROR("Intl.PluralRules.prototype must not be configurable.");
}

reportCompare(0, 0);
