// Copyright (C) 2018 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Closes iterators when toString on a key throws.
esid: sec-object.fromentries
features: [Symbol.iterator, Object.fromEntries]
---*/

function DummyError() {}

var returned = false;
var iterable = {
  [Symbol.iterator]: function() {
    var advanced = false;
    return {
      next: function() {
        if (advanced) {
          throw new Test262Error('should only advance once');
        }
        advanced = true;
        return {
          done: false,
          value: {
            0: {
              toString: function() {
                throw new DummyError();
              },
            },
          },
        };
      },
      return: function() {
        if (returned) {
          throw new Test262Error('should only return once');
        }
        returned = true;
      },
    };
  },
};

assert.throws(DummyError, function() {
  Object.fromEntries(iterable);
});

assert(returned, 'iterator should be closed when key toString throws');

reportCompare(0, 0);
