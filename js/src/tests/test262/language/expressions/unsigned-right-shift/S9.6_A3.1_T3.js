// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator uses ToNumber
es5id: 9.6_A3.1_T3
description: Type(x) is String
---*/

// CHECK#1
if ((new String(1) >>> 0) !== 1) {
  throw new Test262Error('#1: (new String(1) >>> 0) === 1. Actual: ' + ((new String(1) >>> 0)));
}

// CHECK#2
if (("-1.234" >>> 0) !== 4294967295) {
  throw new Test262Error('#2: ("-1.234" >>> 0) === 4294967295. Actual: ' + (("-1.234" >>> 0)));
}

reportCompare(0, 0);
