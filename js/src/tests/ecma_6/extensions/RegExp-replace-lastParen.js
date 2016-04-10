var BUGNUMBER = 1263118;
var summary = "RegExp.prototype[@@replace] with non-standard $+ substitution.";

print(BUGNUMBER + ": " + summary);

assertEq(/(a)(b)(c)/[Symbol.replace]("abc", "[$+]"), "[c]");
assertEq(/(a)(b)c/[Symbol.replace]("abc", "[$+]"), "[b]");
assertEq(/(a)bc/[Symbol.replace]("abc", "[$+]"), "[a]");
assertEq(/abc/[Symbol.replace]("abc", "[$+]"), "[]");

if (typeof reportCompare === "function")
    reportCompare(true, true);
