// |jit-test| exitstatus: 6; skip-if: getBuildConfiguration('pbl')
var global = 0;
setInterruptCallback(function() {
    foo("A");
});
function foo(x) {
    for (var i = 0; i < 1000; i++) {
	var stack = getBacktrace({args: true});
    }
    if (global > 2) return;
    global++;
    interruptIf(true);
    foo("B");
    (function()  { g = x;});
}
foo("C");
