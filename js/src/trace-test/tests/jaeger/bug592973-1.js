// vim: set ts=4 sw=4 tw=99 et:
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

