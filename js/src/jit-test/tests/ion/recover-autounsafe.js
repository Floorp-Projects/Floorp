// |jit-test| --ion-eager; --ion-offthread-compile=off

// Some AutoUnsafeCallWithABI functions can be reached via recovery instructions.
// This testcase is designed to trigger all of the recovery paths that can reach
// AutoUnsafeCallWithABI functions, while an exception is being thrown.

(function() {
    inputs = [];
    f = (function(x) {
	var o = {a: x};
        4294967297 ** (x >>> 0) *
	    4294967297 / x >>> 0 *
	    4294967297 % x >>> 0 *
	    Math.max(4294967297, x >>> 0) *
	    Math.min(4294967, x >>> 0) *
	    Math.atan2(4294967, x >>> 0) *
	    Math.sin(x >>> 0) *
	    Math.sqrt(x >>> 0) *
	    Math.hypot(4294967, x >>> 0) *
	    Math.ceil((x >>> 0) * 0.5) *
	    Math.floor((x >>> 0) * 0.5) *
	    Math.trunc((x >>> 0) * 0.5) *
	    Math.round((x >>> 0) * 0.5) *
	    Math.sign(x >>> 0) *
	    Math.log(x >>> 0) *
	    !o *
            Math.fround(y); // Exception thrown here; y is not defined.
    });
    if (f) {
        for (var j = 0; j < 2; ++j) {
            try {
                f(inputs[0]);
            } catch (e) {}
        }
    }
})();
