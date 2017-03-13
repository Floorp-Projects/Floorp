// This file was procedurally generated from the following sources:
// - src/generators/yield-spread-arr-single.case
// - src/generators/default/method-definition.template
/*---
description: Use yield value in a array spread position (Generator method)
esid: prod-GeneratorMethod
flags: [generated]
includes: [compareArray.js]
info: |
    14.4 Generator Function Definitions

    GeneratorMethod[Yield, Await]:
      * PropertyName[?Yield, ?Await] ( UniqueFormalParameters[+Yield, ~Await] ) { GeneratorBody }

    Array Initializer

    SpreadElement[Yield, Await]:
      ...AssignmentExpression[+In, ?Yield, ?Await]

---*/
var arr = ['a', 'b', 'c'];

var callCount = 0;

var gen = {
  *method() {
    callCount += 1;
    yield [...yield];
  }
}.method;

var iter = gen();

iter.next(false);
var item = iter.next(['a', 'b', 'c']);

assert(compareArray(item.value, arr));
assert.sameValue(item.done, false);

assert.sameValue(callCount, 1);

reportCompare(0, 0);
