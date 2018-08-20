// Copyright (C) 2018 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Throws when called without an argument.
esid: sec-object.fromentries
features: [Object.fromEntries]
---*/

assert.throws(TypeError, function() {
  Object.fromEntries();
});

reportCompare(0, 0);
