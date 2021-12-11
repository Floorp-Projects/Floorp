// Check that duplicate bindings are not created for let/const variables.

let g = newGlobal({newCompartment: true});
let dbg = new Debugger(g);

g.eval(`
function f(x, y=x) {
    let z = "Z";
    debugger;
    return x + y + z;
}
`);

let hits = 0;
let names = [];

dbg.onDebuggerStatement = frame => {
    hits++;
    for (let env = frame.environment; env.type !== "object"; env = env.parent) {
        names.push(...env.names());
    }
};

assertEq(g.f("X", "Y"), "XYZ");
assertEq(hits, 1);
assertEq(names.sort().join(", "), "arguments, x, y, z");
