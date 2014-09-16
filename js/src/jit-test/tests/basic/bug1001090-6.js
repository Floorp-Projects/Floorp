// |jit-test| error: ReferenceError
(function() {
    ((function() {
        p(y)
    })());
    let y
})()
