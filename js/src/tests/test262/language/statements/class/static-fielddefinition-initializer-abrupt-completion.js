// |reftest| skip -- class-fields-public is not supported
// Copyright (C) 2017 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Class construction should error if evaluation of static field initializer errors
esid: runtime-semantics-class-definition-evaluation
info: |
  Runtime Semantics: ClassDefinitionEvaluation
  ...
  33. Let result be InitializeStaticFields(F).
  34. If result is an abrupt completion, then
    a. Set the running execution context's LexicalEnvironment to lex.
    b. Return Completion(result).

  InitializeStaticFields(F)
    1. Assert: Type(F) is Object.
    2. Assert: F is an ECMAScript function object.
    3. Let fieldRecords be the value of F's [[Fields]] internal slot.
    4. For each item fieldRecord in order from fieldRecords,
      a. If fieldRecord.[[static]] is true, then
        i. Perform ? DefineField(F, fieldRecord).

  DefineField(receiver, fieldRecord)
    1. Assert: Type(receiver) is Object.
    2. Assert: fieldRecord is a Record as created by ClassFieldDefinitionEvaluation.
    3. Let fieldName be fieldRecord.[[Name]].
    4. Let initializer be fieldRecord.[[Initializer]].
    5. If initializer is not empty, then
        a. Let initValue be ? Call(initializer, receiver).

features: [class, class-fields-public]
---*/

function f() {
  throw new Test262Error();
}

assert.throws(Test262Error, function() {
  class C {
    static x = f();
  }
})

reportCompare(0, 0);
