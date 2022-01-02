// vim: set ts=8 sts=4 et sw=4 tw=99:
function f(x) {
    if (x) {
        let y;
        y = 12;
        (function () {
          assertEq(y, 12);
        })();
    }
}
f(1);

