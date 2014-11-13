// Closing legacy generators should not invoke the onExceptionUnwind hook.

var g = newGlobal();
var dbg = Debugger(g);
dbg.onExceptionUnwind = function (frame, exc) {
    log += "ERROR";
    assertEq(0, 1);
};
g.eval(`
var log = "";
function f() {
    function gen() {
        try {
            log += "yield";
            yield 3;
            yield 4;
        } catch(e) {
            log += "catch";
        } finally {
            log += "finally";
        }
    };
    var it = gen();
    assertEq(it.next(), 3);
    it.close();
};
f();
`);
assertEq(g.log, "yieldfinally");
