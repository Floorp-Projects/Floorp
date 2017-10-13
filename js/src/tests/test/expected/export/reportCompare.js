// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
  assert.sameValue
esid: pending
---*/


var a = 42;

// comment
assert.sameValue(trueish, true, "ok");

assert.sameValue ( true, /*lol*/true, "ok");

assert.sameValue(true, f instanceof Function);
assert.sameValue(true, true, "don't crash");
assert.sameValue(42, foo);

    // this was a assert.sameValue Line

