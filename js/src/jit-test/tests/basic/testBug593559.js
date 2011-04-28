var gen = (function () {yield})();
var t = gen.throw;
try {
    new t;
} catch (e) {
    actual = "" + e;
}
assertEq(actual, "TypeError: t is not a constructor");
