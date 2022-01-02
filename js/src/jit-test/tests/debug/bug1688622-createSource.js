// Test calling createSource on a non-debuggee global.
for (const global of Debugger().findAllGlobals()) {
    try {
        global.createSource(13);
    } catch (e) {
        assertEq(e.message, "Debugger.Object is not a debuggee global");
    }
}
