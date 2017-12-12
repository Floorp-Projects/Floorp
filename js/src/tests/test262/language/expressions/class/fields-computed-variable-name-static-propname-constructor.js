// |reftest| skip -- class-fields-public is not supported
// This file was procedurally generated from the following sources:
// - src/class-fields/static-propname-constructor.case
// - src/class-fields/propname-error-static/cls-expr-static-variable-name.template
/*---
description: static class field forbid PropName 'constructor' (no early error -- PropName of ComputedPropertyName not forbidden value)
esid: sec-class-definitions-static-semantics-early-errors
features: [class, class-fields-public]
flags: [generated]
info: |
    Static Semantics: PropName
    ...
    ComputedPropertyName : [ AssignmentExpression ]
      Return empty.

    
    // This test file tests the following early error:
    Static Semantics: Early Errors

      ClassElement : staticFieldDefinition;
        It is a Syntax Error if PropName of FieldDefinition is "prototype" or "constructor".

---*/


var constructor = 'foo';
var C = class {
  static [constructor];
};

assert.sameValue(C.hasOwnProperty("foo"), true);

var c = new C();
assert.sameValue(c.hasOwnProperty("foo"), false);

reportCompare(0, 0);
