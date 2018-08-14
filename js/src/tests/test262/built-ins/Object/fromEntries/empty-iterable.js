// Copyright (C) 2018 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: When given an empty list, makes an empty object.
esid: sec-object.fromentries
features: [Object.fromEntries]
---*/

var result = Object.fromEntries([]);
assert.sameValue(Object.keys(result).length, 0);

reportCompare(0, 0);
