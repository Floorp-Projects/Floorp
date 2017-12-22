// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

(function () {
    var [x, y] = [function () { return y; }, 13];
    assertEq(x(), 13);
})();

reportCompare(0, 0, 'ok');
