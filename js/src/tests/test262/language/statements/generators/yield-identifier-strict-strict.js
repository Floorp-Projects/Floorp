// |reftest| error:SyntaxError
'use strict';
// This file was procedurally generated from the following sources:
// - src/generators/yield-identifier-strict.case
// - src/generators/default/statement.template
/*---
description: It's an early error if the generator body has another function body with yield as an identifier in strict mode. (Generator function declaration)
esid: prod-GeneratorDeclaration
flags: [generated, onlyStrict]
negative:
  phase: early
  type: SyntaxError
info: |
    14.4 Generator Function Definitions

    GeneratorDeclaration[Yield, Await, Default]:
      function * BindingIdentifier[?Yield, ?Await] ( FormalParameters[+Yield, ~Await] ) { GeneratorBody }
---*/

var callCount = 0;

function *gen() {
  callCount += 1;
  (function() {
      var yield;
      throw new Test262Error();
    }())
}

var iter = gen();



assert.sameValue(callCount, 1);
