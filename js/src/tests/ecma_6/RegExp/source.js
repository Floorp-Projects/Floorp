var BUGNUMBER = 1120169;
var summary = "Implement RegExp.prototype.source";

print(BUGNUMBER + ": " + summary);

// source is moved to RegExp instance property again (bug 1138325), but keep
// following behavior.
assertEq(RegExp.prototype.source, "(?:)");
assertEq(/foo/.source, "foo");
assertEq(/foo/iymg.source, "foo");
assertEq(/\//.source, "\\/");
assertEq(/\n\r/.source, "\\n\\r");
assertEq(/\u2028\u2029/.source, "\\u2028\\u2029");
assertEq(RegExp("").source, "(?:)");
assertEq(RegExp("", "mygi").source, "(?:)");
assertEq(RegExp("/").source, "\\/");
assertEq(RegExp("\n\r").source, "\\n\\r");
assertEq(RegExp("\u2028\u2029").source, "\\u2028\\u2029");

if (typeof reportCompare === "function")
    reportCompare(true, true);
