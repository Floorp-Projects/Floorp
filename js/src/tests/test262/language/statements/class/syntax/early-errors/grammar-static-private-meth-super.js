// |reftest| skip error:SyntaxError -- class-static-methods-private is not supported
// This file was procedurally generated from the following sources:
// - src/class-elements/grammar-static-private-meth-super.case
// - src/class-elements/syntax/invalid/cls-decl-elements-invalid-syntax.template
/*---
description: Static Private Methods cannot contain direct super (class declaration)
esid: prod-ClassElement
features: [class-static-methods-private, class]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Class Definitions / Static Semantics: Early Errors

    ClassElement : static MethodDefinition
        It is a Syntax Error if HasDirectSuper of MethodDefinition is true.

---*/


throw "Test262: This statement should not be evaluated.";

class C extends Function{
  static #method() {
      super();
  }
}
