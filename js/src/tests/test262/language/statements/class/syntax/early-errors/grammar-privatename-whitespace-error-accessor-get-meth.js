// |reftest| skip error:SyntaxError -- class-methods-private is not supported
// This file was procedurally generated from the following sources:
// - src/class-elements/grammar-privatename-whitespace-error-accessor-get-meth.case
// - src/class-elements/syntax/invalid/cls-decl-elements-invalid-syntax.template
/*---
description: No space allowed between sigil and IdentifierName () (class declaration)
esid: prod-ClassElement
features: [class-methods-private, class]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Updated Productions

    ClassElementName :
      PropertyName
      PrivateName

    PrivateName ::
      # IdentifierName

---*/


throw "Test262: This statement should not be evaluated.";

class C {
  get # m() {}
}
