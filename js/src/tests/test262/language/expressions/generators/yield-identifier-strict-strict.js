// |reftest| error:SyntaxError
'use strict';
// This file was procedurally generated from the following sources:
// - src/generators/yield-identifier-strict.case
// - src/generators/default/expression.template
/*---
description: It's an early error if the generator body has another function body with yield as an identifier in strict mode. (Generator expression)
esid: prod-GeneratorExpression
flags: [generated, onlyStrict]
negative:
  phase: early
  type: SyntaxError
info: |
    14.4 Generator Function Definitions

    GeneratorExpression:
      function * BindingIdentifier[+Yield, ~Await]opt ( FormalParameters[+Yield, ~Await] ) { GeneratorBody }
---*/

var callCount = 0;

var gen = function *() {
  callCount += 1;
  (function() {
      var yield;
      throw new Test262Error();
    }())
};

var iter = gen();



assert.sameValue(callCount, 1);
