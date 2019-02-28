// A Debugger can {return:} from onEnterFrame at any resume point in an async function.
// The async function's promise is resolved with the returned value.

let g = newGlobal({newCompartment: true});
g.eval(`async function f(x) { await x; }`);

let dbg = new Debugger(g);
function test(when) {
    let hits = 0;
    dbg.onEnterFrame = frame => {
        if (frame.type == "call" && frame.callee.name === "f") {
            if (hits++ == when) {
                return {return: "exit"};
            }
        }
    };

    let result = undefined;
    let finished = false;
    g.f("hello").then(value => { result = value; finished = true; });
    drainJobQueue();
    assertEq(finished, true);
    assertEq(hits, when + 1);
    assertEq(result, "exit");
}

// onEnterFrame with hits==0 is not a resume point; {return:} behaves differently there
// (see onEnterFrame-async-resumption-02.js).
test(1);  // force return from first resume point, immediately after the await instruction
