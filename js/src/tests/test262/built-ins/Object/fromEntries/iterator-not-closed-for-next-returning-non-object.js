// Copyright (C) 2018 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Does not close iterators with a `next` method which returns a non-object.
esid: sec-object.fromentries
features: [Symbol.iterator, Object.fromEntries]
---*/

var iterable = {
  [Symbol.iterator]: function() {
    return {
      next: function() {
        return null;
      },
      return: function() {
        throw new Test262Error('should not call return');
      },
    };
  },
};

assert.throws(TypeError, function() {
  Object.fromEntries(iterable);
});

reportCompare(0, 0);
