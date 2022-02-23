// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: no argument searches for a undefined value
---*/

assertEq(#[0].includes(), false, "#[0].includes()");
assertEq(#[undefined].includes(), true, "#[undefined].includes()");

reportCompare(0, 0);
