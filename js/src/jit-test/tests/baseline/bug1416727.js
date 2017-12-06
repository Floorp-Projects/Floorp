// |jit-test| error: too much recursion
g = newGlobal()
g.parent = this
g.eval("new Debugger(parent).onExceptionUnwind = function () {}")
function test() {
    function f(n) {
        if (n != 0) {
            f(n - 1);
        }
        try {
            test();
        } finally {}
    }
    f(100);
}
test();
