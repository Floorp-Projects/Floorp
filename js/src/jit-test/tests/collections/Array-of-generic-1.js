// Array.of can be transplanted to other classes.

load(libdir + "asserts.js");

var hits = 0;
function Bag() {
    hits++;
}
Bag.of = Array.of;

hits = 0;
var actual = Bag.of("zero", "one");
assertEq(hits, 1);

var expected = new Bag;
expected[0] = "zero";
expected[1] = "one";
expected.length = 2;
assertDeepEq(actual, expected);

hits = 0;
actual = Array.of.call(Bag, "zero", "one");
assertEq(hits, 1);
assertDeepEq(actual, expected);

