// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When the Object function is called with one argument value,
    and the value neither is null nor undefined, and is supplied, return ToObject(value)
es5id: 15.2.1.1_A2_T3
description: Calling Object function with string argument value
---*/

var str = 'Luke Skywalker';

assert.sameValue(typeof str, 'string', 'The value of `typeof str` is expected to be "string"');

var obj = Object(str);

assert.sameValue(obj.constructor, String, 'The value of obj.constructor is expected to equal the value of String');
assert.sameValue(typeof obj, "object", 'The value of `typeof obj` is expected to be "object"');
assert(obj == "Luke Skywalker", 'The result of evaluating (obj == "Luke Skywalker") is expected to be true');
assert.notSameValue(obj, "Luke Skywalker", 'The value of obj is not "Luke Skywalker"');

reportCompare(0, 0);
