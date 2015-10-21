// |jit-test| error: ReferenceError
(function() {
    const x = "";
    x = y;
    return x = z;
})();
