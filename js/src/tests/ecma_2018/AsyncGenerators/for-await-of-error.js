var BUGNUMBER = 1391519;
var summary = "for-await-of outside of async function should provide better error";

print(BUGNUMBER + ": " + summary);

let caught = false;
try {
    eval("for await (let x of []) {}");
} catch(e) {
    assertEq(e.message.includes("for await (... of ...) is only valid in"), true);
    caught = true;
}
assertEq(caught, true);

// Extra `await` shouldn't throw that error.
caught = false;
try {
    eval("async function f() { for await await (let x of []) {} }");
} catch(e) {
    assertEq(e.message, "missing ( after for");
    caught = true;
}
assertEq(caught, true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
