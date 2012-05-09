function g() { assertEq(false, true) }
var ct = 0;

function f(b) {
    var a = arguments;
    if (b)
        f(false);
    else
        g = {
            apply:function(x,y) {
                ++ct;
                assertEq(x, null);
                assertEq(typeof y[0], "boolean");
            }
        };
    g.apply(null, a);
}
f(true);
assertEq(ct, 2);
