// |jit-test| error: InternalError: too much recursion
for (let y in []);
(function f(x) {
    new Float64Array(new ArrayBuffer());
    {
        f(x)
        function t() {}
    }
})();
