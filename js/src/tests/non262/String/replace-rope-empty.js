// |reftest| skip-if(!xulRuntime.shell)

var BUGNUMBER = 1509768;
var summary = "String#replace with an empty string pattern on a rope should prepend the replacement string.";

print(BUGNUMBER + ": " + summary);

// Rope is created when the string length >= 25.
//
// This testcase depends on that condition to reliably test the code for
// String#replace on a rope.
//
// Please rewrite this testcase when the following assertion fails.
assertEq(isRope("a".repeat(24)), false);
assertEq(isRope("a".repeat(25)), true);

// Not a rope.
assertEq("a".repeat(24).replace("", "foo"),
         "foo" + "a".repeat(24));
assertEq("a".repeat(24).replace("", ""),
         "a".repeat(24));

// A rope.
assertEq("a".repeat(25).replace("", "foo"),
         "foo" + "a".repeat(25));
assertEq("a".repeat(25).replace("", ""),
         "a".repeat(25));

if (typeof reportCompare === "function")
    reportCompare(true, true);
