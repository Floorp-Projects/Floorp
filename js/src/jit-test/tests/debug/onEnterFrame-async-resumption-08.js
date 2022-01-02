// Terminate execution from within the onPop handler, the result promise will
// never be resolved.

load(libdir + "array-compare.js");

let g = newGlobal({newCompartment: true});
let dbg = new Debugger(g);

let log = [];
g.log = log;

g.eval(`
    async function f() {
        log.push("START");
        await 0;
        log.push("MIDDLE");
        await 0;
        log.push("END");
    }
`);

const expectedTick1 = ["START"];
const expectedTickN = [
    "START",
    "enter: f",
    "MIDDLE",
    "pop: f",
];

Promise.resolve(0)
.then(() => assertEq(arraysEqual(log, expectedTick1), true))
.then(() => assertEq(arraysEqual(log, expectedTickN), true))
.then(() => assertEq(arraysEqual(log, expectedTickN), true))
.then(() => assertEq(arraysEqual(log, expectedTickN), true))
.then(() => assertEq(arraysEqual(log, expectedTickN), true))
.catch(e => {
    // Quit with non-zero exit code to ensure a test suite error is shown,
    // even when this function is called within promise handlers which normally
    // swallow any exceptions.
    print("Error: " + e + "\nstack:\n" + e.stack);
    quit(1);
});

g.f();

dbg.onEnterFrame = frame => {
    log.push(`enter: ${frame.callee.name}`);

    frame.onPop = () => {
        log.push(`pop: ${frame.callee.name}`);
        return null; // Terminate execution.
    };
};
