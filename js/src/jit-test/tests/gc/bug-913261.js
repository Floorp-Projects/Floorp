// |jit-test| error: InternalError: too much recursion
(function f() {
    "".watch(2, function() {});
    f();
})()
