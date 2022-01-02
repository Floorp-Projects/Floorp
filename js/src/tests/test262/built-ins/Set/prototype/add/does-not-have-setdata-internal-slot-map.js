// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.1
description: >
    Set.prototype.add ( value )

    ...
    3. If S does not have a [[SetData]] internal slot, throw a TypeError exception.
    ...

---*/

assert.throws(TypeError, function() {
  Set.prototype.add.call(new Map(), 1);
});

assert.throws(TypeError, function() {
  var s = new Set();
  s.add.call(new Map(), 1);
});

reportCompare(0, 0);
