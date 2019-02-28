// |reftest| skip -- class-fields-private,class-fields-public is not supported
// This file was procedurally generated from the following sources:
// - src/class-elements/private-field-super-access-throws.case
// - src/class-elements/default/cls-expr.template
/*---
description: Acessing private field from super should throw an error (field definitions in a class expression)
esid: prod-FieldDefinition
features: [class-fields-private, class-fields-public, class]
flags: [generated]
info: |
    Updated Productions

    CallExpression[Yield, Await]:
      CoverCallExpressionAndAsyncArrowHead[?Yield, ?Await]
      SuperCall[?Yield, ?Await]
      CallExpression[?Yield, ?Await]Arguments[?Yield, ?Await]
      CallExpression[?Yield, ?Await][Expression[+In, ?Yield, ?Await]]
      CallExpression[?Yield, ?Await].IdentifierName
      CallExpression[?Yield, ?Await]TemplateLiteral[?Yield, ?Await]
      CallExpression[?Yield, ?Await].PrivateName

---*/


var C = class {
  #m = function() { return 'test262'; };

  Child = class extends C {
    access() {
      return super.#m;
    }

    method() {
      return super.#m();
    }
  }

}

assert.throws(TypeError, function() {
  (new (new C()).Child()).method();
}, 'super.#m() throws TypeError');

assert.throws(TypeError, function() {
  (new (new C()).Child()).access();
}, 'super.#m throws TypeError');


reportCompare(0, 0);
