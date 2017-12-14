// |reftest| skip error:SyntaxError -- class-fields-public is not supported
// This file was procedurally generated from the following sources:
// - src/class-fields/static-propname-prototype.case
// - src/class-fields/propname-error-static/cls-expr-static-string-name.template
/*---
description: static class fields forbid PropName 'prototype' (early error -- PropName of StringLiteral is forbidden)
esid: sec-class-definitions-static-semantics-early-errors
features: [class, class-fields-public]
flags: [generated]
negative:
  phase: early
  type: SyntaxError
info: |
    Static Semantics: PropName
    ...
    LiteralPropertyName : StringLiteral
      Return the String value whose code units are the SV of the StringLiteral.

    
    // This test file tests the following early error:
    Static Semantics: Early Errors

      ClassElement : staticFieldDefinition;
        It is a Syntax Error if PropName of FieldDefinition is "prototype" or "constructor".

---*/


throw "Test262: This statement should not be evaluated.";

var C = class {
  static 'prototype';
};
