// |jit-test| --baseline-eager; error: TypeError
try {
    this.__defineGetter__("x", Iterator)()
} catch (e) {}
f = function() {
    return (function() {
        this.x
    })
}()
try {
    f()
} catch (e) {}
f()
