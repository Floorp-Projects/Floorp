// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-64-985cdfad1c7e-linux
// Flags:
//
(function(x){ function a () { x = 2 }; tracing(true); a(); })()
