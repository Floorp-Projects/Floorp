var BUGNUMBER = 1304737;
var summary = "Trailing .* should not be ignored on matchOnly match.";

print(BUGNUMBER + ": " + summary);

function test(r, lastIndexIsZero) {
    r.lastIndex = 0;
    r.test("foo");
    assertEq(r.lastIndex, lastIndexIsZero ? 0 : 3);

    r.lastIndex = 0;
    r.test("foo\nbar");
    assertEq(r.lastIndex, lastIndexIsZero ? 0 : 3);

    var input = "foo" + ".bar".repeat(20000);
    r.lastIndex = 0;
    r.test(input);
    assertEq(r.lastIndex, lastIndexIsZero ? 0 : input.length);

    r.lastIndex = 0;
    r.test(input + "\nbaz");
    assertEq(r.lastIndex, lastIndexIsZero ? 0 : input.length);
}

test(/f.*/, true);
test(/f.*/g, false);
test(/f.*/y, false);
test(/f.*/gy, false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
