// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

(function () {
    var [x, y] = [1, function () { return x; }];
    assertEq(y(), 1);
})();

reportCompare(0, 0, 'ok');
