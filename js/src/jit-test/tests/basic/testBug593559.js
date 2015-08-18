var gen = (function () {yield})();
var t = gen.throw;
try {
    new t;
} catch (e) {
    actual = e;
}
assertEq(actual.name, "TypeError");
assertEq(/is not a constructor/.test(actual.message), true);
