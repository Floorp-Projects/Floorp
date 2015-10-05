/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Currently only a part of async/await grammar is supported:
 * - Async function statements are supported.
 * - Await expressions are supported (as regular unary expressions).
 * All other parts of proposal are probably not supported.
 * Even the supported parts of implementation may not be 100% compliant with
 * the grammar. This is to be considered a proof-of-concept implementation.
 */

if (asyncFunctionsEnabled()) {
    assertEq(Reflect.parse("function a() {}").body[0].async, false);
    assertEq(Reflect.parse("function* a() {}").body[0].async, false);
    assertEq(Reflect.parse("async function a() {}").body[0].async, true);
    assertEq(Reflect.parse("() => {}").body[0].async, undefined);

    // Async generators are not allowed (with regards to spec)
    assertThrows(() => Reflect.parse("async function* a() {}"), SyntaxError);

    // No line terminator after async
    assertEq(Reflect.parse("async\nfunction a(){}").body[0].expression.name, "async");

    // Async arrow functions are allowed, but not supported yet
    assertThrows(() => Reflect.parse("async () => true"), SyntaxError);

    // Async function expressions
    assertEq(Reflect.parse("(async function() {})()").body[0].expression.callee.async, true);
    assertEq(Reflect.parse("var k = async function() {}").body[0].declarations[0].init.async, true);
    assertEq(Reflect.parse("var nmd = async function named() {}").body[0].declarations[0].init.id.name, "named");

    // Awaiting not directly inside an async function is not allowed
    assertThrows(() => Reflect.parse("await 4;"), SyntaxError);
    assertThrows(() => Reflect.parse("function a() { await 4; }"), SyntaxError);
    assertThrows(() => Reflect.parse("function* a() { await 4; }"), SyntaxError);
    assertThrows(() => Reflect.parse("async function k() { function a() { await 4; } }"), SyntaxError);

    // No line terminator after await is allowed
    assertThrows(() => Reflect.parse("async function a() { await\n4; }"), SyntaxError);

    // Await is not allowed as a default expr.
    assertThrows(() => Reflect.parse("async function a(k = await 3) {}"), SyntaxError);
    assertThrows(() => Reflect.parse("async function a() { async function b(k = await 3) {} }"), SyntaxError);

    // Await is not legal as an identifier in an async function.
    assertThrows(() => Reflect.parse("async function a() { var await = 4; }"), SyntaxError);
    assertThrows(() => Reflect.parse("async function a() { return await; }"), SyntaxError);

    // Yield is not allowed in an async function / treated as identifier
    assertThrows(() => Reflect.parse("async function a() { yield 3; }"), SyntaxError);

    // Await is still available as an identifier name in strict mode code.
    Reflect.parse("function a() { 'use strict'; var await = 3; }");
    Reflect.parse("'use strict'; var await = 3;");

    // Await is treated differently depending on context. Various cases.
    Reflect.parse("var await = 3; async function a() { await 4; }");
    Reflect.parse("async function a() { await 4; } var await = 5");
    Reflect.parse("async function a() { function b() { return await; } }")

    Reflect.parse("async function a() { var k = { async: 4 } }");

    Reflect.parse("function a() { await: 4 }");

    assertEq(Reflect.parse("async function a() { await 4; }")
        .body[0].body.body[0].expression.operator, "await");

    assertEq(Reflect.parse("async function a() { async function b() { await 4; } }")
        .body[0].body.body[0].body.body[0].expression.operator, "await");

    // operator priority test
    assertEq(Reflect.parse("async function a() { await 2 + 3; }")
        .body[0].body.body[0].expression.left.argument.value, 2);
    assertEq(Reflect.parse("async function a() { await 2 + 3; }")
        .body[0].body.body[0].expression.left.operator, "await");
    assertEq(Reflect.parse("async function a() { await 2 + 3; }")
        .body[0].body.body[0].expression.right.value, 3);

    // blocks and other constructions
    assertEq(Reflect.parse("{ async function a() { return 2; } }")
        .body[0].body[0].async, true);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
