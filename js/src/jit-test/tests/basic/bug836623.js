
x = []
Object.defineProperty(x, 4, {
    configurable: true
})
Array.prototype.pop.call(x)
for (let y = 0; y < 9; ++y) {
    Object.defineProperty(x, 7, {
        configurable: true,
        enumerable: (y != 2),
        writable: true
    })
}
