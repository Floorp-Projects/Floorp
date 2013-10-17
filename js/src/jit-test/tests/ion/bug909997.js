var wait = 100;

var method_A = function() {
    for (var t = 0; t < wait; ++t) {}
}

var method_B = function() {
    for (var t = 0; t < wait; ++t) {}
}

var method_C = function() {
    for (var t = 0; t < wait; ++t) {}
}

var method_D = function() {
    for (var t = 0; t < wait; ++t) {}
}

var func = [method_A, method_B, method_C, method_D]
var opts = ["baseline.enable", "ion.enable"];

for (var n = 0; n < opts.length; ++n) {
    for (var m = 0; m < 2; ++m) {
        setJitCompilerOption(opts[n], m & 1);
        for (var i = 0; i < 1001; ++i)
            func[n*2+m]();
    }
}