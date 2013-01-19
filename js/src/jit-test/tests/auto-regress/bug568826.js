// Binary: cache/js-dbg-64-fadb38356e0f-linux
// Flags: -j
//
NaN = []
for (var a = 0; a < 2; ++a) {
    if (a == 1) {
        for each(e in [NaN, Infinity, NaN]) {}
    }
}
