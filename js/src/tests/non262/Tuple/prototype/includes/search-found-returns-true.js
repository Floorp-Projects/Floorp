// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: returns true for found index
---*/

var symbol = Symbol("1");
var tuple = #[];
var record = #{};

var sample = #[42, "test262", null, undefined, true, false, 0, -1, "", symbol, tuple, record];

assertEq(sample.includes(42), true, "42");
assertEq(sample.includes("test262"), true, "'test262'");
assertEq(sample.includes(null), true, "null");
assertEq(sample.includes(undefined), true, "undefined");
assertEq(sample.includes(true), true, "true");
assertEq(sample.includes(false), true, "false");
assertEq(sample.includes(0), true, "0");
assertEq(sample.includes(-1), true, "-1");
assertEq(sample.includes(""), true, "the empty string");
assertEq(sample.includes(symbol), true, "symbol");
assertEq(sample.includes(tuple), true, "tuple");
assertEq(sample.includes(record), true, "record");

reportCompare(0, 0);
