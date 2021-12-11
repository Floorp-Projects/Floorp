// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: >
  Number,Boolean,Symbol cannot have own enumerable properties,
  So cannot be Assigned.Here result should be original object.
esid: sec-object.assign
features: [Symbol]
---*/

var target = new Object();
var result = Object.assign(target, 123, true, Symbol('foo'));

assert.sameValue(result, target, "Numbers, booleans, and symbols cannot have wrappers with own enumerable properties.");

reportCompare(0, 0);
