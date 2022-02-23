// |jit-test| skip-if: !this.hasOwnProperty("ReadableStream")
// Test uncatchable error when a stream source's pull() method is called.

// Make `debugger;` raise an uncatchable error.
let g = newGlobal({newCompartment: true});
g.parent = this;
g.hit = false;
g.eval(`
    new Debugger(parent).onDebuggerStatement = _frame => (hit = true, null);
`);

// Create a stream whose pull() method raises an uncatchable error,
// and try reading from it.
let readerCreated = false;
let fnFinished = false;
async function fn() {
    try {
        let stream = new ReadableStream({
            start(controller) {},
            pull(controller) {
                debugger;
            }
        });

        let reader = stream.getReader();
        let p = reader.read();
        readerCreated = true;
        await p;
    } finally {
        fnFinished = true;
    }
}

fn();
drainJobQueue();
assertEq(readerCreated, true);
assertEq(g.hit, true);
assertEq(fnFinished, false);
