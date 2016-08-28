var BUGNUMBER = 1185106;
var summary = "yield handling in async function";

print(BUGNUMBER + ": " + summary);

function testPassArgsBody(argsbody) {
    Reflect.parse(`async function a${argsbody}`);
    Reflect.parse(`(async function a${argsbody})`);
    Reflect.parse(`(async function ${argsbody})`);
    Reflect.parse(`({ async m${argsbody} })`);
}

function testErrorArgsBody(argsbody, prefix="") {
    assertThrows(() => Reflect.parse(`${prefix} async function a${argsbody}`), SyntaxError);
    assertThrows(() => Reflect.parse(`${prefix} (async function a${argsbody})`), SyntaxError);
    assertThrows(() => Reflect.parse(`${prefix} (async function ${argsbody})`), SyntaxError);
    assertThrows(() => Reflect.parse(`${prefix} ({ async m${argsbody} })`), SyntaxError);
}

function testErrorArgsBodyStrict(argsbody) {
    testErrorArgsBody(argsbody);
    testErrorArgsBody(argsbody, "'use strict'; ");
    assertThrows(() => Reflect.parse(`class X { async m${argsbody} }`), SyntaxError);
    assertThrows(() => Reflect.parse(`class X { static async m${argsbody} }`), SyntaxError);
    assertThrows(() => Reflect.parse(`export default async function ${argsbody}`, { target: "module" }), SyntaxError);
}

if (typeof Reflect !== "undefined" && Reflect.parse) {
    // `yield` handling is inherited in async function declaration name.
    Reflect.parse("async function yield() {}");
    Reflect.parse("function f() { async function yield() {} }");
    assertThrows(() => Reflect.parse("function* g() { async function yield() {} }"), SyntaxError);
    assertThrows(() => Reflect.parse("'use strict'; async function yield() {}"), SyntaxError);

    // `yield` is treated as an identifier in an async function expression name.
    // `yield` is not allowed as an identifier in strict code.
    Reflect.parse("(async function yield() {});");
    Reflect.parse("function f() { (async function yield() {}); }");
    Reflect.parse("function* g() { (async function yield() {}); }");
    assertThrows(() => Reflect.parse("'use strict'; (async function yield() {});"), SyntaxError);

    // `yield` handling is inherited in async method name.
    Reflect.parse("({ async yield() {} });");
    Reflect.parse("function f() { ({ async yield() {} }); }");
    Reflect.parse("function* g() { ({ async yield() {} }); }");
    Reflect.parse("'use strict'; ({ async yield() {} });");
    Reflect.parse("class X { async yield() {} }");

    Reflect.parse("({ async [yield]() {} });");
    Reflect.parse("function f() { ({ async [yield]() {} }); }");
    Reflect.parse("function* g() { ({ async [yield]() {} }); }");
    assertThrows(() => Reflect.parse("'use strict'; ({ async [yield]() {} });"), SyntaxError);
    assertThrows(() => Reflect.parse("class X { async [yield]() {} }"), SyntaxError);

    // `yield` is treated as an identifier in an async function parameter
    // `yield` is not allowed as an identifier in strict code.
    testPassArgsBody("(yield) {}");
    testPassArgsBody("(yield = 1) {}");
    testPassArgsBody("(a = yield) {}");
    testErrorArgsBodyStrict("(yield 3) {}");
    testErrorArgsBodyStrict("(a = yield 3) {}");

    // `yield` is treated as an identifier in an async function body
    // `yield` is not allowed as an identifier in strict code.
    testPassArgsBody("() { yield; }");
    testPassArgsBody("() { yield = 1; }");
    testErrorArgsBodyStrict("() { yield 3; }");
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
