g = newGlobal()
g.parent = this
g.eval("new Debugger(parent).onExceptionUnwind = function () {}")
enableSPSProfiling()

try {
  // Only the ARM simulator supports single step profiling.
  enableSingleStepProfiling();
} catch (e) {
  quit();
}

function assertThrowsInstanceOf(f) {
    try {
        f()
    } catch (exc) {}
}
function testThrow(thunk) {
    for (i = 0; i < 20; i++) {
        iter = thunk()
        assertThrowsInstanceOf(function() iter.throw())
    }
}
testThrow(function*() {})
