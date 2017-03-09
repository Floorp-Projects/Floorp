// Copyright 2016 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: sec-Intl.PluralRules
description: Tests that the this-value is ignored in PluralRules
author: Zibi Braniecki
includes: [testIntl.js]
---*/

testWithIntlConstructors(function (Constructor) {
    var obj, newObj;

    // variant 1: use constructor in a "new" expression
    obj = new Constructor();
    newObj = Intl.PluralRules.call(obj);
    if (obj === newObj) {
      $ERROR("PluralRules object created with \"new\" was not ignored as this-value.");
    }

    // variant 2: use constructor as a function
    obj = Constructor();
    newObj = Intl.PluralRules.call(obj);
    if (obj === newObj) {
      $ERROR("PluralRules object created with constructor as function was not ignored as this-value.");
    }

    return true;
});

reportCompare(0, 0);
