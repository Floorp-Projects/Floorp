"use strict";
function f() {
    var o = {};
    Object.defineProperty(o, "x", {get: undefined, set: undefined});
    for (var i = 0; i < 20; i++) {
        var ex = null;
        try {
            o.x = 9;
        } catch (e) {
            ex = e;
        }
        assertEq(ex instanceof TypeError, true);
        assertEq(o.x, undefined);
    }
}
f();
