
setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 30);
var i;

var uceFault = function (i) {
    if (i > 98)
        uceFault = function (i) { return true; };
    return false;
}

var sqrt5 = Math.sqrt(5);
var phi = (1 + sqrt5) / 2;
function range_analysis_truncate(i) {
    var fib = (Math.pow(phi, i) - Math.pow(1 - phi, i)) / sqrt5;
    var x = (fib >> 8) * (fib >> 6);
    if (uceFault(i) || uceFault(i))
        assertEq(x, (fib >> 8) * (fib >> 6));
    return x | 0;
}

for (i = 0; i < 100; i++) {
    range_analysis_truncate(i);
}
