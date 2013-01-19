// Binary: cache/js-dbg-64-c08baee44cf4-linux
// Flags: -j
//
try {
    with({
        x: (function f(a) {
            f(1)
        })()
    }) {}
} catch(e) {}
for each(x in ["", true]) {
    for (b = 0; b < 4; ++b) {
        if (b % 2 == 0) {
            (function () {})()
        } {
            gc()
        }
    }
}
