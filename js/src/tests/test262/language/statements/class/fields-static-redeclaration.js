// |reftest| skip -- class-fields-public is not supported
// This file was procedurally generated from the following sources:
// - src/class-fields/static-redeclaration.case
// - src/class-fields/default/cls-decl.template
/*---
description: Redeclaration of public static fields with the same name (field definitions in a class declaration)
esid: prod-FieldDefinition
features: [class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js, compareArray.js]
info: |
    2.13.2 Runtime Semantics: ClassDefinitionEvaluation

    ...
    30. Set the value of F's [[Fields]] internal slot to fieldRecords.
    ...
    33. Let result be InitializeStaticFields(F).

    InitializeStaticFields(F)

    3. Let fieldRecords be the value of F's [[Fields]] internal slot.
    4. For each item fieldRecord in order from fieldRecords,
      a. If fieldRecord.[[static]] is true, then
        i. Perform ? DefineField(F, fieldRecord).

---*/
var x = [];


class C {
  static y = (x.push("a"), "old_value");
  static ["y"] = (x.push("b"), "another_value");
  static "y" = (x.push("c"), "same_value");
  static y = (x.push("d"), "same_value");
}

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "y"), false);

verifyProperty(C, "y", {
  value: "same_value",
  enumerable: true,
  writable: true,
  configurable: true
});

assert(compareArray(x, ["a", "b", "c", "d"]));

var c = new C();
assert.sameValue(Object.hasOwnProperty.call(c, "y"), false);

assert(compareArray(x, ["a", "b", "c", "d"]), "static fields are not redefined on class instatiation");

reportCompare(0, 0);
