var BUGNUMBER = 1185106;
var summary = "async arrow function syntax";

print(BUGNUMBER + ": " + summary);

if (typeof Reflect !== "undefined" && Reflect.parse) {
    // Parameters.
    Reflect.parse("async () => 1");
    Reflect.parse("async a => 1");
    Reflect.parse("async (a) => 1");
    Reflect.parse("async async => 1");
    Reflect.parse("async (async) => 1");
    Reflect.parse("async ([a]) => 1");
    Reflect.parse("async ([a, b]) => 1");
    Reflect.parse("async ({a}) => 1");
    Reflect.parse("async ({a, b}) => 1");

    assertThrowsInstanceOf(() => Reflect.parse("async await => 1"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async (await) => 1"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async ([await]) => 1"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async ({await}) => 1"), SyntaxError);

    assertThrowsInstanceOf(() => Reflect.parse("async (a=await) => 1"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async ([a=await]) => 1"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async ({a=await}) => 1"), SyntaxError);

    assertThrowsInstanceOf(() => Reflect.parse("async (a=await 1) => 1"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async ([a=await 1]) => 1"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async ({a=await 1}) => 1"), SyntaxError);

    assertThrowsInstanceOf(() => Reflect.parse("async [a] => 1"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async [a, b] => 1"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async {a} => 1"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async {a: b} => 1"), SyntaxError);

    // Expression body.
    Reflect.parse("async a => a == b");

    // Expression body with nested async function.
    Reflect.parse("async a => async");
    Reflect.parse("async a => async b => c");
    Reflect.parse("async a => async function() {}");
    Reflect.parse("async a => async function b() {}");

    assertThrowsInstanceOf(() => Reflect.parse("async a => async b"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async a => async function"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async a => async function()"), SyntaxError);

    // Expression body with `await`.
    Reflect.parse("async a => await 1");
    Reflect.parse("async a => await await 1");
    Reflect.parse("async a => await await await 1");

    assertThrowsInstanceOf(() => Reflect.parse("async a => await"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("async a => await await"), SyntaxError);

    // `await` is Unary Expression and it cannot have `async` function as an
    // operand.
    assertThrowsInstanceOf(() => Reflect.parse("async a => await async X => Y"), SyntaxError);
    Reflect.parse("async a => await (async X => Y)");
    // But it can have `async` identifier as an operand.
    Reflect.parse("async async => await async");

    // Block body.
    Reflect.parse("async X => {yield}");

    // `yield` handling.
    Reflect.parse("async X => yield");
    Reflect.parse("async yield => X");
    Reflect.parse("async yield => yield");
    Reflect.parse("async X => {yield}");

    Reflect.parse("async X => {yield}");
    Reflect.parse("async yield => {X}");
    Reflect.parse("async yield => {yield}");
    Reflect.parse("function* g() { async X => yield }");

    assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async yield => X"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async (yield) => X"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async X => yield"), SyntaxError);
    assertThrowsInstanceOf(() => Reflect.parse("'use strict'; async X => {yield}"), SyntaxError);

    assertThrows(() => Reflect.parse("function* g() { async yield => X }"));
    assertThrows(() => Reflect.parse("function* g() { async (yield) => X }"));
    assertThrows(() => Reflect.parse("function* g() { async ([yield]) => X }"));
    assertThrows(() => Reflect.parse("function* g() { async ({yield}) => X }"));

    // Not async functions.
    Reflect.parse("async ()");
    Reflect.parse("async (a)");
    Reflect.parse("async (async)");
    Reflect.parse("async ([a])");
    Reflect.parse("async ([a, b])");
    Reflect.parse("async ({a})");
    Reflect.parse("async ({a, b})");

    // Async arrow function is assignment expression.
    Reflect.parse("a ? async () => {1} : b");
    Reflect.parse("a ? b : async () => {1}");
    assertThrowsInstanceOf(() => Reflect.parse("async () => {1} ? a : b"), SyntaxError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
