// |jit-test| error: ReferenceError
(function() {
    let V = x(x, x = w), x
})()
