// |jit-test| error: ReferenceError
var t = {};
function r(y) t.y = y;
function g() {
    for (let [x = r(x)] of x) {}
}
r(0);
r(0);
g();
