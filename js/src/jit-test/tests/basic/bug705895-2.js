// |jit-test| error: TypeError
function f(o) {
    for (j = 0; j < 9; j++) {
        if (j) {
            o.__proto__ = null
        }
        for (v in o) {}
    }
}
for (i = 0; i < 9; i++) {
    (new Boolean).__proto__.__defineGetter__("toString", function() {})
    f(Boolean.prototype)
}
