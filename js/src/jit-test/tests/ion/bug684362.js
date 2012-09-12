// vim: set ts=4 sw=4 tw=99 et:
function f(a) {
    var k = a;
    T: for (;;) {
        for (;;) {
            for (;;) {
                if (k)
                    continue;
                break T;
            }
        }
    }
}
f(0);
