/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

var symbols = [
    Symbol(),
    Symbol("0"),
    Symbol.for("snowman"),
    Symbol.iterator
];

var values = [
    undefined, null, 0, 3.14, -0, NaN, "", "alphabet", Symbol("0"),
    {}, []
];

for (var comparator of ["==", "!=", "===", "!==", "<", "<=", ">", ">="]) {
    var f = Function("a, b", "return a " + comparator + " b;");
    var expected = (comparator[0] == '!');
    for (var a of symbols) {
        for (var b of values)
            assertEq(f(a, b), expected);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
