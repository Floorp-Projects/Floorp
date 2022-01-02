enableOsiPointRegisterChecks()
function f() {}
f.__defineGetter__("x", (function() {
    this._
}))
for (var i = 0; i < 3; i++) {
    (function() {
        for (var j = 0; j < 1; j++) {
            f.x + 1
        }
    })()
}
