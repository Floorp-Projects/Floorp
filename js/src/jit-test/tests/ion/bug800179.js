// |jit-test| error: TypeError

try {
    x = []
    y = function() {}
    t = Uint8ClampedArray
    Object.defineProperty(x, 1, {
        get: (function() {
            for (v of t) {}
        })
    })
    Object.defineProperty(x, 8, {
        configurable: t
    }).reverse()
} catch (e) {}
Object.defineProperty([], 1, {
    configurable: true,
    get: (function() {
        for (j = 0; j < 50; ++j) {
            y()
        }
    })
}).pop()
x.map(y)
