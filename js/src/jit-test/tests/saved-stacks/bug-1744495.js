// |jit-test| --fast-warmup; --more-compartments

enableTrackAllocations()
e = function(a) {
    b = newGlobal()
    c = new b.Debugger
    return function(d, code) {
        c.addDebuggee(a)
        c.getNewestFrame().older.older.eval(code)
    }
}(this)
for (var i = 0; i < 50; i++) f()
function g() {
    e(1, "a.push0")
}
function f() {
    g()
}
