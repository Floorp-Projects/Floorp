// |reftest| skip -- class-fields-public is not supported
// This file was procedurally generated from the following sources:
// - src/class-fields/static-computed-name-toprimitive.case
// - src/class-fields/default/cls-decl.template
/*---
description: ToPrimitive evaluation in the ComputedPropertyName (field definitions in a class declaration)
esid: prod-FieldDefinition
features: [computed-property-names, Symbol.toPrimitive, class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js]
info: |
    Runtime Semantics: ClassDefinitionEvaluation

    ...
    27. For each ClassElement e in order from elements
      a. If IsStatic of me is false, then
        i. Let fields be the result of performing ClassElementEvaluation for e with arguments proto and false.
      b. Else,
        i. Let fields be the result of performing ClassElementEvaluation for e with arguments F and false.
      c. If fields is an abrupt completion, then
        i. Set the running execution context's LexicalEnvironment to lex.
        ii. Set the running execution context's PrivateNameEnvironment to outerPrivateEnvironment.
        iii. Return Completion(status).
    ...

    Runtime Semantics: ClassElementEvaluation

    ClassElement: static FieldDefinition;
      Return ClassFieldDefinitionEvaluation of FieldDefinition with parameter true and object.

    ClassElement: FieldDefinition;
      Return ClassFieldDefinitionEvaluation of FieldDefinition with parameter false and object.

    Runtime Semantics: ClassFieldDefinitionEvaluation
      With parameters isStatic and homeObject.

    1. Let fieldName be the result of evaluating ClassElementName.
    2. ReturnIfAbrupt(fieldName).
    ...

    Runtime Semantics: Evaluation
      ComputedPropertyName: [ AssignmentExpression ]

    1. Let exprValue be the result of evaluating AssignmentExpression.
    2. Let propName be ? GetValue(exprValue).
    3. Return ? ToPropertyKey(propName).

---*/
var err = function() { throw new Test262Error(); };
var obj1 = {
  [Symbol.toPrimitive]: function() { return "d"; },
  toString: err,
  valueOf: err
};

var obj2 = {
  toString: function() { return "e"; },
  valueOf: err
};

var obj3 = {
  toString: undefined,
  valueOf: function() { return "f"; }
};



class C {
  static [obj1] = 42;
  static [obj2] = 43;
  static [obj3] = 44;
}

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "d"), false);
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "e"), false);
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "f"), false);

verifyProperty(C, "d", {
  value: 42,
  enumerable: true,
  writable: true,
  configurable: true
});

verifyProperty(C, "e", {
  value: 43,
  enumerable: true,
  writable: true,
  configurable: true
});

verifyProperty(C, "f", {
  value: 44,
  enumerable: true,
  writable: true,
  configurable: true
});

var c = new C();

assert.sameValue(Object.hasOwnProperty.call(c, "d"), false);
assert.sameValue(Object.hasOwnProperty.call(c, "e"), false);
assert.sameValue(Object.hasOwnProperty.call(c, "f"), false);

reportCompare(0, 0);
