// |jit-test| error: TypeError
function test(stdlib, foreign) {
    "use asm"
    var ff = foreign.ff
    function f(y) {
        y = +y;
        ff(0);
    }
    return f;
};
f = test(this, {ff: Object.preventExtensions});
f();
