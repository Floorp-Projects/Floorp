var BUGNUMBER = 1335025;
var summary = "(non-standard) async function toSource";

print(BUGNUMBER + ": " + summary);

async function f1(a, b, c) { await a; }

assertEq(f1.toSource(),
         "async function f1(a, b, c) { await a; }");

assertEq(async function (a, b, c) { await a; }.toSource(),
         "(async function (a, b, c) { await a; })");

assertEq((async (a, b, c) => await a).toSource(),
         "async (a, b, c) => await a");

assertEq((async (a, b, c) => { await a; }).toSource(),
         "async (a, b, c) => { await a; }");

assertEq({ async foo(a, b, c) { await a; } }.foo.toSource(),
         "async foo(a, b, c) { await a; }");

if (typeof reportCompare === "function")
    reportCompare(true, true);
