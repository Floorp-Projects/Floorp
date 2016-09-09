// |jit-test| exitstatus:6
timeout(1);
// Adapted from randomly chosen test: js/src/jit-test/tests/asm.js/testBug975182.js
(function() {
    g = (function(t, foreign) {
        "use asm";
        var ff = foreign.ff;
        function f() {
            ff()
        }
        return f
    })(this, {
        ff: arguments.callee
    })
})()
function m(f) {
    while (true) {
        f();
    }
}
m(g);
