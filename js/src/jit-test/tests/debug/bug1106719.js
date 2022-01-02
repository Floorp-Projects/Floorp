// |jit-test| allow-oom; allow-unhandlable-oom; allow-overrecursed; skip-if: getBuildConfiguration()['android']
// Disabled on Android due to harness problems (Bug 1532654)

g = newGlobal({newCompartment: true})
g.parent = this
g.eval("Debugger(parent).onExceptionUnwind=(function(){})")
gcparam("maxBytes", gcparam("gcBytes"))
function f() {
    f()
    y(arguments)
}
f()
