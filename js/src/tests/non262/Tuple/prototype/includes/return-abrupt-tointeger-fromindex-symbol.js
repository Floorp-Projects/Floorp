// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Return abrupt from ToInteger(fromIndex) - using symbol
---*/

var fromIndex = Symbol("1");

var sample = #[7];

assertThrowsInstanceOf(() => sample.includes(7, fromIndex), TypeError);

reportCompare(0, 0);
