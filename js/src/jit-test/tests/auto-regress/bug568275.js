// |jit-test| need-for-each

// Binary: cache/js-dbg-32-9ca0a738a8ad-linux
// Flags:
//
x = ({
    x: /x/
})
x.p = []
Object.defineProperty(x, "x", {
    writable: ""
})
for (b in [[], null, [undefined]]) {
    function f(a) {
        delete a.x
        a.x = Infinity
    }
    for each(z in [x]) {
        f(z)
    }
}
