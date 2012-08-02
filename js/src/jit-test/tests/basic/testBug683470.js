// |jit-test| debug

f = (function() {
    function b() {
        "use strict";
        Object.defineProperty(this, "x", ({}));
    }
    for each(let d in [0, 0]) {
        try {
            b(d);
        } catch (e) {}
    }
})
trap(f, 40, undefined);
f();
