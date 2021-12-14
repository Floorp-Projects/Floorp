// Resume execution of async generator when initially yielding.

let g = newGlobal({newCompartment: true});
let dbg = new Debugger();
let gw = dbg.addDebuggee(g);

g.eval(`
    async function* f() {
        await 123;
        return "ponies";
    }
`);

let counter = 0;
let thenCalled = false;
dbg.onEnterFrame = frame => {
    frame.onPop = completion => {
        if (counter++ === 0) {
            let genObj = completion.return.unsafeDereference();

            // The following call enqueues the request before it becomes
            // suspendedStart, that breaks the assumption in the spec,
            // and there's no correct interpretation.
            genObj.next().then(() => {
                thenCalled = true;
            });
        }
    };
};

let it = g.f();

assertEq(counter, 1);

let caught = false;
try {
  // The async generator is already in the invalid state, and the following
  // call fails.
  it.next();
} catch (e) {
  caught = true;
}
assertEq(caught, true);

caught = false;
try {
  it.throw();
} catch (e) {
  caught = true;
}
assertEq(caught, true);

caught = false;
try {
  it.return();
} catch (e) {
  caught = true;
}
assertEq(caught, true);

drainJobQueue();

assertEq(thenCalled, false);
