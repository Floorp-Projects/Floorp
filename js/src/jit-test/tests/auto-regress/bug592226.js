// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-e8ee411dca70-linux
// Flags:
//
x = Proxy.create((function () {
    return {
        get: Object.create
    }
})([]), "")
try {
    (function () {
        for each(l in [0]) {
            print(x)
        }
    })()
} catch (e) {}
gc()
for each(let a in [0]) {
    print(x)
}
