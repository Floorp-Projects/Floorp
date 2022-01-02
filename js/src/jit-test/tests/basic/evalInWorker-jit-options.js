// |jit-test| skip-if: helperThreadCount() === 0
var fun = function() {
    var ex;
    try {
        setJitCompilerOption("baseline.warmup.trigger", 5);
    } catch(e) {
        ex = e;
    }
    assertEq(ex && ex.toString().includes("Can't set"), true);
}
evalInWorker("(" + fun.toString() + ")()");
