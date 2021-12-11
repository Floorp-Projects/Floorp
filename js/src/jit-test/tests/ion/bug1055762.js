// |jit-test| error: TypeError
function g() {
    f(0);
}
function f(y) {
    return (undefined <= y);
}
try {
    g();
} catch (e) {}
(function() {
    f()()
})();
