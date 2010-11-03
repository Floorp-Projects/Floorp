var gen = (function () {yield})();
var t = gen.throw;

try {
    new t;
} catch (e) {
    actual = "" + e;
}
assertEq(actual, "TypeError: Generator.prototype.throw called on incompatible Object");

