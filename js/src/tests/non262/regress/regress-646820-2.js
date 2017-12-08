// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

(function () {
    var obj = {prop: 1};
    var [x, {prop: y}] = [function () { return y; }, obj];
    assertEq(y, 1);
    assertEq(x(), 1);
})();

reportCompare(0, 0, 'ok');
