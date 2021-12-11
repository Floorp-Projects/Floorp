// vim: set ts=8 sts=4 et sw=4 tw=99:
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
