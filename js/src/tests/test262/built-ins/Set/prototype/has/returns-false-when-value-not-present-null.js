// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.7
description: >
    Set.prototype.has ( value )

    ...
    6. Return false.

---*/

var s = new Set();

assert.sameValue(s.has(null), false, "`s.has(null)` returns `false`");

reportCompare(0, 0);
