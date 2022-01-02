// |jit-test| error: ReferenceError
(function(x) {
    x = i ? 4 : 2
    y
})()
