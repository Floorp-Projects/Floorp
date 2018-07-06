// A Debugger can {return:} from onDebuggerStatement in a generator.

load(libdir + "asserts.js");

let g = newGlobal();
g.eval(`
    function* f1() {
        debugger;
        yield 1;
    }

    function* f2() {
        yield 1;
        debugger;
        yield 2;
    }
`);

let dbg = Debugger(g);
dbg.onDebuggerStatement = frame => ({return: "Ronja"});

let genObj = g.f1();
assertDeepEq(genObj.next(), {value: "Ronja", done: true});
// Forced return closes the generator. Closed generators behave like this:
assertDeepEq(genObj.next(), {value: undefined, done: true});

// It works the same after resuming from a yield.
genObj = g.f2();
assertDeepEq(genObj.next(), {value: 1, done: false});
assertDeepEq(genObj.next(), {value: "Ronja", done: true});
assertDeepEq(genObj.next(), {value: undefined, done: true});  // closed

