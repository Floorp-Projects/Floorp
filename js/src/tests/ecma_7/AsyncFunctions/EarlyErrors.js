var BUGNUMBER = 1185106;
var summary = "EarlyErrors for async function";

print(BUGNUMBER + ": " + summary);

function assertThrowsSE(code) {
  assertThrows(() => Reflect.parse(code), SyntaxError);
}

if (typeof Reflect !== "undefined" && Reflect.parse) {
    // If FormalParameters Contains AwaitExpression is true.
    assertThrowsSE("async function a(k = await 3) {}");

    // If BindingIdentifier is `eval` or `arguments`.
    assertThrowsSE("'use strict'; async function eval() {}");

    assertThrowsSE("'use strict'; async function arguments() {}");

    // If any element of the BoundNames of FormalParameters also occurs in the
    // LexicallyDeclaredNames of AsyncFunctionBody.
    assertThrowsSE("async function a(x) { let x; }");

    // If FormalParameters contains SuperProperty is true.
    assertThrowsSE("async function a(k = super.prop) { }");

    // If AsyncFunctionBody contains SuperProperty is true.
    assertThrowsSE("async function a() { super.prop(); }");

    // If FormalParameters contains SuperCall is true.
    assertThrowsSE("async function a(k = super()) {}");

    // If AsyncFunctionBody contains SuperCall is true.
    assertThrowsSE("async function a() { super(); }");
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
