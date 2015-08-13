// |jit-test| allow-oom; allow-overrecursed

g = newGlobal()
g.parent = this
g.eval("Debugger(parent).onExceptionUnwind=(function(){})")
gcparam("maxBytes", gcparam("gcBytes"))
function f() {
    f()
    y(arguments)
}
f()
