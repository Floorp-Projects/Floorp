// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.2
description: >
    Set.prototype.clear ( )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof Set.prototype.clear,
  "function",
  "`typeof Set.prototype.clear` is `'function'`"
);

verifyNotEnumerable(Set.prototype, "clear");
verifyWritable(Set.prototype, "clear");
verifyConfigurable(Set.prototype, "clear");

reportCompare(0, 0);
