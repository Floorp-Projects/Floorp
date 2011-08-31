
function f(o) {
    o[{}] = 1;
    with(Object) {}
}
f(Object.prototype);
