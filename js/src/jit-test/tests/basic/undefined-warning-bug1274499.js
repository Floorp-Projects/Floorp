options("strict", "werror");

var o = {};
var failureCode = 0;

try {
    // Don't throw here.
    if (o.a || o.b)
        failureCode = 1;
} catch (e) {
    failureCode = 2
}
assertEq(failureCode, 0);
