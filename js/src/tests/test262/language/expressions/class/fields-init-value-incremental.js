// |reftest| skip -- class-fields-public is not supported
// This file was procedurally generated from the following sources:
// - src/class-fields/init-value-incremental.case
// - src/class-fields/default/cls-expr.template
/*---
description: The initializer value is defined during the class instatiation (field definitions in a class expression)
esid: prod-FieldDefinition
features: [computed-property-names, class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js]
info: |
    Runtime Semantics: ClassDefinitionEvaluation

    27. For each ClassElement e in order from elements
      ...
      d. Append to fieldRecords the elements of fields.
    ...
    33. Let result be InitializeStaticFields(F).
    ...

    [[Construct]] ( argumentsList, newTarget)

    8. If kind is "base", then
      a. Perform OrdinaryCallBindThis(F, calleeContext, thisArgument).
      b. Let result be InitializeInstanceFields(thisArgument, F).
      ...
    ...
    11. Let result be OrdinaryCallEvaluateBody(F, argumentsList).
    ...

---*/
var x = 0;


var C = class {
  static [x++] = x++;
  [x++] = x++;
  static [x++] = x++;
  [x++] = x++;
}

var c1 = new C();
var c2 = new C();

verifyProperty(C, "0", {
  value: 4,
  enumerable: true,
  configurable: true,
  writable: true,
});

verifyProperty(C, "2", {
  value: 5,
  enumerable: true,
  configurable: true,
  writable: true,
});

verifyProperty(c1, "1", {
  value: 6,
  enumerable: true,
  configurable: true,
  writable: true,
});

verifyProperty(c1, "3", {
  value: 7,
  enumerable: true,
  configurable: true,
  writable: true,
});

verifyProperty(c2, "1", {
  value: 8,
  enumerable: true,
  configurable: true,
  writable: true,
});

verifyProperty(c2, "3", {
  value: 9,
  enumerable: true,
  configurable: true,
  writable: true,
});

reportCompare(0, 0);
