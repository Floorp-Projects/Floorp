// |jit-test| error: TypeError
function f(o) {
    o.watch("x", this);
}
var c = evalcx("");
f(c);
