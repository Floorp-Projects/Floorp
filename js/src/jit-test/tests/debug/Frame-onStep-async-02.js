// With enough hackery, stepping in and out of async functions can be made to
// work as users expect.
//
// This test exercises the common case when we have syntactically `await
// $ASYNC_FN($ARGS)` so that the calls nest as if they were synchronous
// calls. It works, but there's a problem.
//
// onStep fires in extra places that end users would find very confusing--see
// the comment marked (!) below. As a result, Debugger API consumers must do
// some extra work to skip pausing there. This test is a proof of concept that
// shows what sort of effort is needed. It maintains a single `asyncStack` and
// skips the onStep hook if we're not running the function at top of the async
// stack. Real debuggers would have to maintain multiple async stacks.

// Set up debuggee.
var g = newGlobal({newCompartment: true});
g.eval(`\
async function outer() {                                // line 1
    return (await inner()) + (await inner()) + "!";     // 2
}                                                       // 3
async function inner() {                                // 4
    return (await leaf()) + (await leaf());             // 5
}                                                       // 6
async function leaf() {                                 // 7
    return (await Promise.resolve("m"));                // 8
}                                                       // 9
`);

// Set up debugger.
let previousLine = -1;
let dbg = new Debugger(g);
let log = "";
let asyncStack = [];

dbg.onEnterFrame = frame => {
    assertEq(frame.type, "call");

    // If we're entering this frame for the first time, push it to the async
    // stack.
    if (!frame.seen) {
        frame.seen = true;
        asyncStack.push(frame);
        log += "(";
    }

    frame.onStep = () => {
        // When stepping, we sometimes pause at opcodes in older frames (!)
        // where all that's happening is async function administrivia.
        //
        // For example, the first time `leaf()` yields, `inner()` and
        // `outer()` are still on the stack; they haven't awaited yet because
        // control has not returned from `leaf()` to them yet. So stepping will
        // hop from line 8 to line 5 to line 2 as we unwind the stack, then
        // resume on line 8.
        //
        // Anyway: skip that noise.
        if (frame !== asyncStack[asyncStack.length - 1])
            return;

        let line = frame.script.getOffsetLocation(frame.offset).lineNumber;
        if (previousLine != line) {
            log += line; // We stepped to a new line.
            previousLine = line;
        }
    };

    frame.onPop = completion => {
        // Popping the frame. But async function frames are popped multiple
        // times: for the "initial suspend", at each await, and on return. The
        // debugger offers no easy way to distinguish them (bug 1470558).
        // For now there's an "await" property, but bug 1470558 may come up
        // with a different solution, so don't rely on it!
        if (!completion.await) {
            // Returning (not awaiting or at initial suspend).
            assertEq(asyncStack.pop(), frame);
            log += ")";
        }
    };
};

// Run.
let result;
g.outer().then(v => { result = v; });
drainJobQueue();

assertEq(result, "mmmm!");
assertEq(log, "(12(45(789)5(789)56)2(45(789)5(789)56)23)");
