if (asyncFunctionsEnabled()) {
    parseModule("async function f() { await 3; }");
    parseModule("async function f() { await 3; }");
    assertThrows(() => parseModule("var await = 5;"), SyntaxError);
    assertThrows(() => parseModule("export var await;"), SyntaxError);
    assertThrows(() => parseModule("async function f() { function g() { await 3; } }"), SyntaxError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
