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

for (var n = 0; n < 4; ++n) {
    try {
	setJitCompilerOption("baseline.enable", n & 1);
	setJitCompilerOption("ion.enable", n & 2 ? 1: 0);
    } catch(e) {
	if (e.toString().contains("on the stack"))
	    continue;
	throw e;
    }
    var opt = getJitCompilerOptions();
    assertEq(opt["baseline.enable"], n & 1);
    assertEq(opt["ion.enable"], n & 2 ? 1 : 0);
    for (var i = 0; i < 1001; ++i)
        func[n]();
}
