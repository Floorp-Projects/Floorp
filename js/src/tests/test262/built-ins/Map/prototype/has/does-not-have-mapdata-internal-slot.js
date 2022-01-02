// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.7
description: >
  Throws a TypeError if `this` object does not have a [[MapData]] internal slot.
info: |
  Map.prototype.has ( key )

  ...
  3. If M does not have a [[MapData]] internal slot, throw a TypeError
  exception.
  ...
---*/

var m = new Map();

assert.throws(TypeError, function() {
  Map.prototype.has.call([], 1);
});

assert.throws(TypeError, function() {
  m.has.call([], 1);
});

assert.throws(TypeError, function() {
  Map.prototype.has.call({}, 1);
});

assert.throws(TypeError, function() {
  m.has.call({}, 1);
});

reportCompare(0, 0);
