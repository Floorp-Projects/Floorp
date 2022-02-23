// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Tuple.prototype.includes.name is "includes".
---*/

assertEq(Tuple.prototype.includes.name, "includes");
var desc = Object.getOwnPropertyDescriptor(Tuple.prototype.includes, "name");

assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

reportCompare(0, 0);
