g = newGlobal()
g.parent = this
g.eval("new Debugger(parent).onExceptionUnwind = function(){}");
var depth = 0;
function test() {
    if (++depth > 50)
        return;
    function f(n) {
        if (n != 0) {
            f(n - 1);
            return;
        }
        try {
            test();
        } finally {}
    }
    f(80);
}
try {
    test();
} catch(e) {}
