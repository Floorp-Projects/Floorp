var BUGNUMBER = 887016;
var summary = "RegExp.prototype[@@split] should check if this value is RegExp.";

print(BUGNUMBER + ": " + summary);

if (Symbol.split) {
    var obj = { flags: "", toString: () => "-" };
    assertDeepEq(RegExp.prototype[Symbol.split].call(obj, "a-b-c"),
                 ["a", "b", "c"]);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);

