// |reftest| skip error:SyntaxError -- class-methods-private is not supported
// This file was procedurally generated from the following sources:
// - src/generators/yield-as-binding-identifier.case
// - src/generators/syntax/class-expr-private-method.template
/*---
description: yield is a reserved keyword within generator function bodies and may not be used as a binding identifier. (Generator private method as a ClassExpression element)
esid: prod-GeneratorMethod
features: [generators, class-methods-private]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    ClassElement :
      PrivateMethodDefinition

    MethodDefinition :
      GeneratorMethod

    14.4 Generator Function Definitions

    GeneratorMethod :
      * # PropertyName ( UniqueFormalParameters ) { GeneratorBody }


    BindingIdentifier : Identifier

    It is a Syntax Error if this production has a [Yield] parameter and
    StringValue of Identifier is "yield".

---*/
throw "Test262: This statement should not be evaluated.";

var C = class {*#gen() {
    var yield;
}};
