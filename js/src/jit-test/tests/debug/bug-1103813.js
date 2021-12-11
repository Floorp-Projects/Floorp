// Random chosen test: js/src/jit-test/tests/debug/Source-invisible.js
newGlobal({
    newCompartment: true,
    invisibleToDebugger: true
})
// Random chosen test: js/src/jit-test/tests/debug/Debugger-findObjects-05.js
x = (new Debugger).findObjects()
