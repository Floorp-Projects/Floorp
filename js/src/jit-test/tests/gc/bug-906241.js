// |jit-test| error: InternalError: too much recursion
for (let y in []);
(function f(x) {
    Float64Array(ArrayBuffer());
    {
        f(x)
        function t() {}
    }
})();
