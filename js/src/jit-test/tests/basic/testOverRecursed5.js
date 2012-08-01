JSON.__proto__[1] = Uint8ClampedArray().buffer
f = (function() {
    function g(c) {
        Object.freeze(c).__proto__ = c
    }
    for each(b in []) {
        try {
            g(b)
        } catch (e) {}
    }
})
f()
f()
