// |jit-test| error:ReferenceError
function f() {
        (x ? Math.fround(0) : x ? a : x) && b;
}
f(Math.fround);
