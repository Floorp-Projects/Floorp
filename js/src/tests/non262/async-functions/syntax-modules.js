var BUGNUMBER = 1185106;
var summary = "async/await syntax in module";

print(BUGNUMBER + ": " + summary);

if (typeof parseModule === "function") {
    parseModule("async function f() { await 3; }");
    parseModule("async function f() { await 3; }");
    assertThrowsInstanceOf(() => parseModule("function f() { await 5; }"), SyntaxError);
    assertThrowsInstanceOf(() => parseModule("() => { await 5; }"), SyntaxError);
    assertThrowsInstanceOf(() => parseModule("export var await;"), SyntaxError);
    assertThrowsInstanceOf(() => parseModule("await => 1;"), SyntaxError);
    assertThrowsInstanceOf(() => parseModule("async function f() { function g() { await 3; } }"), SyntaxError);

    if (typeof Reflect !== "undefined" && Reflect.parse) {
        Reflect.parse("export async function f() {}", { target: "module" });
        assertThrowsInstanceOf(() => Reflect.parse("export async function() {}", { target: "module" }), SyntaxError);

        Reflect.parse("export default async function() {}", { target: "module" });
        Reflect.parse("export default async function f() {}", { target: "module" });

        assertThrowsInstanceOf(() => Reflect.parse("export default async function() { yield; }", { target: "module" }), SyntaxError);
        assertThrowsInstanceOf(() => Reflect.parse("export default async function() { yield = 1; }", { target: "module" }), SyntaxError);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
