// If the debuggee cannot be put into debug mode, throw.

// Run this test only if this compartment can't be put into debug mode.
var canEnable = true;
if (typeof setDebugMode === 'function') {
    try {
        setDebugMode(true);
    } catch (exc) {
        canEnable = false;
    }
}

if (!canEnable) {
    var g = newGlobal();
    g.libdir = libdir;
    g.eval("load(libdir + 'asserts.js');");
    g.parent = this;
    g.eval("assertThrowsInstanceOf(function () { new Debugger(parent); }, Error);");
}
