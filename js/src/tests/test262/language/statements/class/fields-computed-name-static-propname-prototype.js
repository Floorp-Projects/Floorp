// |reftest| skip -- class-fields-public is not supported
// Copyright (C) 2017 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Cannot redefine a non-configurable, non-writable "prototype" property
esid: runtime-semantics-class-definition-evaluation
features: [class, class-fields-public]
info: |
  // This test file also tests the ComputedPropertyName won't trigger the
  // following early error:
  Static Semantics: Early Errors

    ClassElement : staticFieldDefinition;
      It is a Syntax Error if PropName of FieldDefinition is "prototype" or
      "constructor".

  2.13.2 Runtime Semantics: ClassDefinitionEvaluation

  ...
  21. Perform MakeConstructor(F, false, proto).
  ...
  25. Else, let elements be NonConstructorMethodDefinitions of ClassBody.
  26. Let fieldRecords be a new empty List.
  26. For each ClassElement me in order from elements
    a. If IsStatic of me is false, then
      i. Let fields be the result of performing ClassElementEvaluation for e
      with arguments proto and false.
    ...
  33. Let result be InitializeStaticFields(F).
  ...

  // ClassElementEvaluation should evaluate the ComputedPropertyName

  12.2.6.7 Runtime Semantics: Evaluation

    ComputedPropertyName:[AssignmentExpression]

    1. Let exprValue be the result of evaluating AssignmentExpression.
    2. Let propName be ? GetValue(exprValue).
    3. Return ? ToPropertyKey(propName).

  // And the Static Fields should be defined to the class

  2.8 InitializeStaticFields(F)

  ...
  3. Let fieldRecords be the value of F's [[Fields]] internal slot.
  4. For each item fieldRecord in order from fieldRecords,
    a. If fieldRecord.[[static]] is true, then
      i. Perform ? DefineField(F, fieldRecord).
---*/

assert.throws(TypeError, function() {
  class C {
    static ["prototype"];
  };
});

reportCompare(0, 0);
