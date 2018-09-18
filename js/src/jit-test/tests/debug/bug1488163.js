async function f() {
    // Enough variables that SM will eventually decide to create a lookup table
    // for this scope.
    var q0, q1, q2, q3, q4, q5, q6, q7, q8, q9;
}

var g = newGlobal();
g.parent = this;
g.eval(`
    var dbg = new Debugger(parent);
    dbg.onEnterFrame = frame => {};
`);

// Fragile: Trigger Shape::hashify() for the shape of the environment in f
// under a call to GetGeneratorObjectForFrame from the Debugger.
for (let i = 0; i < 3; i++)
    f();
