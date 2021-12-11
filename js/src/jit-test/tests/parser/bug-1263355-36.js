// |jit-test| error: ReferenceError

(function f() {
    let x = (new function() {
        x(() => {
            f.ArrayType(1, 2);
        }, "first argument of ctypes.cast must be a CData");
    })
})();
