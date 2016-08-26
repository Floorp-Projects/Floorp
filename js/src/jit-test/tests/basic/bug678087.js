load(libdir + "asserts.js");

assertThrowsInstanceOf(function () {
i = 0;
do {
with({
    TestCase: Float64Array
}) for each(let TestCase in [TestCase]) {}
 } while (i++ < 10);
function TestCase(n, d, e, a) {}
}, ReferenceError);
