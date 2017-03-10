// This file was procedurally generated from the following sources:
// - src/generators/yield-spread-arr-single.case
// - src/generators/default/statement.template
/*---
description: Use yield value in a array spread position (Generator function declaration)
esid: prod-GeneratorDeclaration
flags: [generated]
includes: [compareArray.js]
info: |
    14.4 Generator Function Definitions

    GeneratorDeclaration[Yield, Await, Default]:
      function * BindingIdentifier[?Yield, ?Await] ( FormalParameters[+Yield, ~Await] ) { GeneratorBody }

    Array Initializer

    SpreadElement[Yield, Await]:
      ...AssignmentExpression[+In, ?Yield, ?Await]

---*/
var arr = ['a', 'b', 'c'];

var callCount = 0;

function *gen() {
  callCount += 1;
  yield [...yield];
}

var iter = gen();

iter.next(false);
var item = iter.next(['a', 'b', 'c']);

assert(compareArray(item.value, arr));
assert.sameValue(item.done, false);

assert.sameValue(callCount, 1);

reportCompare(0, 0);
