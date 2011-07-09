function g(a, b, c, d) {}
function f(a, b, c) {
        arguments.length=8.64e15;
        g.apply(this, arguments);
}f();
