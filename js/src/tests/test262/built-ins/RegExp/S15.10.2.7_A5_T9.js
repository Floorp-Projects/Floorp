// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production QuantifierPrefix :: ? evaluates by returning the two
    results 0 and 1
es5id: 15.10.2.7_A5_T9
description: Execute /b?b?b?b/.exec("abbbbc") and check results
---*/

var __executed = /b?b?b?b/.exec("abbbbc");

var __expected = ["bbbb"];
__expected.index = 1;
__expected.input = "abbbbc";

assert.sameValue(
  __executed.length,
  __expected.length,
  'The value of __executed.length is expected to equal the value of __expected.length'
);

assert.sameValue(
  __executed.index,
  __expected.index,
  'The value of __executed.index is expected to equal the value of __expected.index'
);

assert.sameValue(
  __executed.input,
  __expected.input,
  'The value of __executed.input is expected to equal the value of __expected.input'
);

for(var index=0; index<__expected.length; index++) {
  assert.sameValue(
    __executed[index],
    __expected[index],
    'The value of __executed[index] is expected to equal the value of __expected[index]'
  );
}

reportCompare(0, 0);
