// |reftest| error:SyntaxError
'use strict';
// This file was procedurally generated from the following sources:
// - src/generators/yield-identifier-spread-strict.case
// - src/generators/default/class-method-definition.template
/*---
description: It's an early error if the AssignmentExpression is a function body with yield as an identifier in strict mode. (Generator method as a ClassElement)
esid: prod-GeneratorMethod
features: [object-spread]
flags: [generated, onlyStrict]
negative:
  phase: early
  type: SyntaxError
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
    return {
         ...(function() {
            var yield;
            throw new Test262Error();
         }()),
      }
}}

var gen = C.prototype.gen;

var iter = gen();



assert.sameValue(callCount, 1);
