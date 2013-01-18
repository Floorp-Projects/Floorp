// Binary: cache/js-dbg-64-441daac3fef9-linux
// Flags: -j
//
function f() {
    this.b = function() {};
    this.b = Object.e;
    Object.defineProperty(this, "b", {})
}
for (a in [0, 0, 0, 0]) {
    new f
}
