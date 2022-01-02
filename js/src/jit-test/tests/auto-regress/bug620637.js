// |jit-test| error:Error

// Binary: cache/js-dbg-64-aeeb631c6d43-linux
// Flags:
//
options('tracejit');
var actual = ''
function f() {
    for (var a = 0; a < 3; ++a) {
        (function () {
            for (var b = 0; b < 2; ++b) {
                (function () {
                    for (a = 0, b = 0; b < 15; b++, actual = actual + "7") {
                    }
                })();
            }
        })();
    }
}
f();
