var x = 0;
var o1 = {x: 1};
var o2 = {};
for (var o of [o1, o2]) {
    Object.defineProperty(o, "thrower", {get: function() {
        if (x++ > 1900) {
            throw "error";
        }
    }});
}
function f() {
    var res = 0;
    try {
        for (var i = 0; i < 2000; i++) {
            res = Math.fround(i - 123456.7);
            var o = (i & 1) ? o1 : o2;
            o.thrower;
            res++;
        }
    } catch(e) {}
    assertEq(res, -121555.703125);
    return res;
}
f();
