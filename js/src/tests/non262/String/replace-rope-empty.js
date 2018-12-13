// |reftest| skip-if(!xulRuntime.shell)

var BUGNUMBER = 1509768;
var summary = "String#replace with an empty string pattern on a rope should prepend the replacement string.";

print(BUGNUMBER + ": " + summary);

// Rope is created when the string length >= 24.
//
// This testcase depends on that condition to reliably test the code for
// String#replace on a rope.
//
// Please rewrite this testcase when the following assertion fails.
assertEq(isRope("a".repeat(23)), false);
assertEq(isRope("a".repeat(24)), true);

// Not a rope.
assertEq("a".repeat(23).replace("", "foo"),
         "foo" + "a".repeat(23));
assertEq("a".repeat(23).replace("", ""),
         "a".repeat(23));

// A rope.
assertEq("a".repeat(24).replace("", "foo"),
         "foo" + "a".repeat(24));
assertEq("a".repeat(24).replace("", ""),
         "a".repeat(24));

if (typeof reportCompare === "function")
    reportCompare(true, true);
