var x;
function f(o) {
    o.prop = x = 3;
}
f({});
f(1);
