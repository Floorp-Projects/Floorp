// This file was procedurally generated from the following sources:
// - src/generators/yield-spread-obj.case
// - src/generators/default/class-method-definition.template
/*---
description: Use yield value in a object spread position (Generator method as a ClassElement)
esid: prod-GeneratorMethod
features: [object-spread]
flags: [generated]
includes: [compareArray.js]
info: |
    ClassElement[Yield, Await]:
      MethodDefinition[?Yield, ?Await]

    MethodDefinition[Yield, Await]:
      GeneratorMethod[?Yield, ?Await]

    14.4 Generator Function Definitions

    GeneratorMethod[Yield, Await]:
      * PropertyName[?Yield, ?Await] ( UniqueFormalParameters[+Yield, ~Await] ) { GeneratorBody }

    Spread Properties

    PropertyDefinition[Yield]:
      (...)
      ...AssignmentExpression[In, ?Yield]

---*/

var callCount = 0;

class C { *gen() {
    callCount += 1;
    yield {
        ...yield,
        y: 1,
        ...yield yield,
      };
}}

var gen = C.prototype.gen;

var iter = gen();

iter.next();
iter.next({ x: 42 });
iter.next({ x: 'lol' });
var item = iter.next({ y: 39 });

assert.sameValue(item.value.x, 42);
assert.sameValue(item.value.y, 39);
assert.sameValue(Object.keys(item.value).length, 2);
assert.sameValue(item.done, false);

assert.sameValue(callCount, 1);

reportCompare(0, 0);
