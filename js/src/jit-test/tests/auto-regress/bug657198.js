// |jit-test| slow;

// Binary: cache/js-dbg-32-599d1c6cba63-linux
// Flags: -j
//
eval("")
o15 = {}
function f11(o) {
    props = Object.getOwnPropertyNames(o)
    prop = props.length ? prop[prop.e] + "" : ""
    o[prop] = 3
}
function f12(o) {
    _someglobal_ = o;
    for (j = 0; j < 5; j++) {
        for (x in {
            x: {
                x: function() {
                    return _someglobal_
                }
            }.x()
        }.x) {
            ({
                x: {
                    x: function() {}
                }.x()
            }[x])
        }
        gc()
    }
} {
    for (i = 0; i < 100; i++) {
        f12(o15)
        f11(o15)
    }
}
