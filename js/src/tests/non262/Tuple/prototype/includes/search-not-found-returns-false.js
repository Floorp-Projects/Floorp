// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: returns false if the element is not found
---*/

assertEq(#[42].includes(43), false, "43");

assertEq(#["test262"].includes("test"), false, "string");

assertEq(#[0, "test262", undefined].includes(""), false, "the empty string");

assertEq(#["true", false].includes(true), false, "true");
assertEq(#["", true].includes(false), false, "false");

assertEq(#[undefined, false, 0, 1].includes(null), false, "null");
assertEq(#[null].includes(undefined), false, "undefined");

assertEq(#[Symbol("1")].includes(Symbol("1")), false, "symbol");

var sample = #[42];
assertEq(sample.includes(sample), false, "this");

reportCompare(0, 0);
