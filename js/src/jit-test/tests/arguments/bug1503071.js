// |jit-test| exitstatus: 6; skip-if: getBuildConfiguration('pbl')
var g = true

setInterruptCallback(function() {
    print(getBacktrace({args: true}));
});

function foo(bt, x=3, y = eval("g")) {
    if (g) {
	g = false
	interruptIf(true);
	foo(false);
    }
    (function()  { n = bt;});
}
foo(false);
