var BUGNUMBER = 1185106;
var summary = "async/await syntax in module";

print(BUGNUMBER + ": " + summary);

if (typeof parseModule === "function") {
    parseModule("async function f() { await 3; }");
    parseModule("async function f() { await 3; }");
    assertThrows(() => parseModule("var await = 5;"), SyntaxError);
    assertThrows(() => parseModule("export var await;"), SyntaxError);
    assertThrows(() => parseModule("async function f() { function g() { await 3; } }"), SyntaxError);

    if (typeof Reflect !== "undefined" && Reflect.parse) {
        assertThrows(() => Reflect.parse("export default async function() { yield; }", { target: "module" }), SyntaxError);
        assertThrows(() => Reflect.parse("export default async function() { yield = 1; }", { target: "module" }), SyntaxError);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
