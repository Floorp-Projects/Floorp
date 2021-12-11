// |jit-test| error: RangeError
function g(a, b, c, d) {}
function f(a, b, c) {
        arguments.length = getMaxArgs() + 1;
        g.apply(this, arguments);
}f();
