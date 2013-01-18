// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-32-525d852c622d-linux
// Flags: -j
//
(function() {
    let(x)
    (function() {
        for (let a in [0, x, 0, 0])
        (function() {
            for (let y in [0, 0]) print
        })();
    })()
    with({}) throw x;
})()
