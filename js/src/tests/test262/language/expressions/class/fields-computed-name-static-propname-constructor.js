// |reftest| skip -- class-fields-public is not supported
// Copyright (C) 2017 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  static class fields forbid PropName 'constructor' (no early error for
  ComputedPropertyName)
esid: sec-class-definitions-static-semantics-early-errors
features: [class, class-fields-public]
info: |
  Static Semantics: PropName
  ...
  ComputedPropertyName : [ AssignmentExpression ]
    Return empty.
  
  // This test file also tests the ComputedPropertyName won't trigger the
  // following early error:
  Static Semantics: Early Errors

    ClassElement : staticFieldDefinition;
      It is a Syntax Error if PropName of FieldDefinition is "prototype" or
      "constructor".
---*/

var C = class {
  static ["constructor"];
};

assert.sameValue(C.hasOwnProperty("constructor"), true);

var c = new C();
assert.sameValue(c.hasOwnProperty("constructor"), false);

reportCompare(0, 0);
