var res;
var x = 0;

function f() {
    x = {x: 1, f: function() { res = this.x; }};
    with(x) {
        g = function() {
            eval("");
            f();
        }
        g();
    }
}
f();
assertEq(res, 1);
