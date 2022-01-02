// |jit-test| error: SyntaxError
// Binary: cache/js-dbg-32-7e713db43d8d-linux
// Flags:
//
f = (function() {
    "use strict";
    [arguments] = eval()
})
