// |jit-test| error: too much recursion
(function() {
    (function f(x) {
        return x * f(x - 1);
        with({})
        var r = ""
    })()
})()

