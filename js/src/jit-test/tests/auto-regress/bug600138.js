// Binary: cache/js-dbg-64-54700fad8cf9-linux
// Flags: -j
//
function z() {
    this.a = function () {}
    this.a = this
    Object.defineProperty(this, "a", ({}))
}
for (e in [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]) {
    new z()
}
