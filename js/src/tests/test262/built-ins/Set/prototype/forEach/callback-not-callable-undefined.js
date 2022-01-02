// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.6
description: >
    Set.prototype.forEach ( callbackfn [ , thisArg ] )

    ...
    4. If IsCallable(callbackfn) is false, throw a TypeError exception.
    ...

    Passing `undefined` as callback

---*/

var s = new Set([1]);

assert.throws(TypeError, function() {
  s.forEach(undefined);
});

reportCompare(0, 0);
