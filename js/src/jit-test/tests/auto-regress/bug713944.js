// Binary: cache/js-dbg-64-7e28cce342a6-linux
// Flags:
//
function f() {
    constructor = {}
}
Object.defineProperty(function() {
    return {}.__proto__
}(), "constructor", {
    set: function() {}
})
f()
Object.defineProperty({}.__proto__, "constructor", {
    value: 3
})
f()
