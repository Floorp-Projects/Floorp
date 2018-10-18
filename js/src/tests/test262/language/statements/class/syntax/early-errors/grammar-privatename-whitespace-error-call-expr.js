// |reftest| skip error:SyntaxError -- class-fields-private is not supported
// This file was procedurally generated from the following sources:
// - src/class-elements/grammar-privatename-whitespace-error-call-expr.case
// - src/class-elements/syntax/invalid/cls-decl-elements-invalid-syntax.template
/*---
description: No space allowed between sigil and IdentifierName (CallExpression) (class declaration)
esid: prod-ClassElement
features: [class-fields-private, class]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Updated Productions

    MemberExpression :
      MemberExpression . PrivateName

    CallExpression :
      CallExpression . PrivateName

    PrivateName ::
      # IdentifierName

---*/


throw "Test262: This statement should not be evaluated.";

class C {
  #x;

  f() {
    return this;
  }

  m() {
    this.f().# x;
  }
}
