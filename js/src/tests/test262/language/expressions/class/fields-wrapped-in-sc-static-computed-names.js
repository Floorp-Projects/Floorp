// |reftest| skip -- class-fields-public is not supported
// This file was procedurally generated from the following sources:
// - src/class-fields/static-computed-names.case
// - src/class-fields/productions/cls-expr-wrapped-in-sc.template
/*---
description: Static Computed property names (fields definition wrapped in semicolons)
esid: prod-FieldDefinition
features: [computed-property-names, class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js]
info: |
    ClassElement:
      ...
      FieldDefinition ;
      static FieldDefinition ;

    FieldDefinition:
      ClassElementName Initializer_opt

    ClassElementName:
      PropertyName

---*/


var C = class {
  ;;;;
  ;;;;;;static ["a"] = 42; ["a"] = 39;;;;;;;
  ;;;;

}

var c = new C();

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "a"), false);

verifyProperty(C, "a", {
  value: 42,
  enumerable: true,
  writable: true,
  configurable: true
});

verifyProperty(c, "a", {
  value: 39,
  enumerable: true,
  writable: true,
  configurable: true
});

reportCompare(0, 0);
