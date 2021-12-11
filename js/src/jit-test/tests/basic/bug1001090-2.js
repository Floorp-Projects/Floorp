// |jit-test| error: ReferenceError
(function() {
    with(x);
    let x
})()
