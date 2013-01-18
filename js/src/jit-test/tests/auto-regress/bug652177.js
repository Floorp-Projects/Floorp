// Binary: cache/js-dbg-32-d5fcfa622f16-linux
// Flags:
//
Function.toLocaleString.__proto__ = null
y = {}.__proto__
y.p = function() {}
y.__defineSetter__("", function() {})
y.__proto__ = Function.toLocaleString
function b(a) {
    this.__proto__ = a;
    Object.freeze(this)
}
for each(z in []) {
    new b
}
