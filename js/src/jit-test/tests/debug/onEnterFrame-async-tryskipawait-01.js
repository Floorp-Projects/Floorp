// CanSkipAwait with resolved Promises when attaching onEnterFrame
// after the initial call into the async function.

load(libdir + "array-compare.js");

let g = newGlobal({ newCompartment: true });
let dbg = new Debugger(g);

let log = [];
g.log = log;

g.eval(`
    async function f() {
        log.push("START");
        await Promise.resolve(0);
        log.push("MIDDLE");
        await Promise.resolve(0);
        log.push("END");
    }
`);

function neverCalled(e) {
    // Quit with non-zero exit code to ensure a test suite error is shown,
    // even when this function is called within promise handlers which normally
    // swallow any exceptions.
    print("Error: " + e + "\nstack:\n" + e.stack);
    quit(1);
}

g.f().then(() => {
    assertEq(arraysEqual(log, [
        "START",
        "enter: f",
        "MIDDLE",
        "END",
    ]), true);
}).catch(neverCalled);

dbg.onEnterFrame = frame => {
    log.push(`enter: ${frame.callee.name}`);
};
