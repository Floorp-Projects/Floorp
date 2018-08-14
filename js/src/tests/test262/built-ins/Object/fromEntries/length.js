// Copyright (C) 2018 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Object.fromEntries.length is 1.
esid: sec-object.fromentries
includes: [propertyHelper.js]
features: [Object.fromEntries]
---*/

assert.sameValue(Object.fromEntries.length, 1);

verifyNotEnumerable(Object.fromEntries, "length");
verifyNotWritable(Object.fromEntries, "length");
verifyConfigurable(Object.fromEntries, "length");

reportCompare(0, 0);
