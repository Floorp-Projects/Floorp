// Accessing Debugger.Script's properties which triggers delazification can
// fail if the function for the script is optimized out.
// It shouldn't crash but just throw an error.

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
g.eval(`
function enclosing() {
    (function g1() {});
    (function* g2() {});
    (async function g3() {});
    (async function* g4() {});
    () => {};
    async () => {};
}
`);

for (const s of dbg.findScripts()) {
    if (!s.displayName) {
        continue;
    }

    try {
        s.lineCount;  // don't assert
    } catch (exc) {
        // If that didn't throw, it's fine. If it did, check the message.
        assertEq(exc.message, "function is optimized out");
    }
}
