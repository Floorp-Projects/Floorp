// |jit-test| skip-if: !this.hasOwnProperty("ReadableStream")
// Test uncatchable error when a stream's queuing strategy's size() method is called.

// Make `debugger;` raise an uncatchable exception.
let g = newGlobal({newCompartment: true});
g.parent = this;
g.hit = false;
g.eval(`
    var dbg = new Debugger(parent);
    dbg.onDebuggerStatement = (_frame, exc) => (hit = true, null);
`);

let fnFinished = false;
async function fn() {
    // Await once to postpone the uncatchable error until we're running inside
    // a reaction job. We don't want the rest of the test to be terminated.
    // (`drainJobQueue` catches uncatchable errors!)
    await 1; 

    try {
        // Create a stream with a strategy whose .size() method raises an
        // uncatchable exception, and have it call that method.
        new ReadableStream({
            start(controller) {
                controller.enqueue("FIRST POST");  // this calls .size()
            }
        }, {
            size() {
                debugger;
            }
        });
    } finally {
        fnFinished = true;
    }
}

fn();
drainJobQueue();
assertEq(g.hit, true);
assertEq(fnFinished, false);
