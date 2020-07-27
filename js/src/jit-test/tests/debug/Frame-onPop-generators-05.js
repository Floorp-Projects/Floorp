// In .onPop for the "initial yield" of a generator, while the generator frame
// is on the stack, the generator object's .next() method throws.

let g = newGlobal({newCompartment: true});
g.eval(`
    function* f() {
        return "ok";
    }
`);

let hits = 0;
let dbg = new Debugger;
let gw = dbg.addDebuggee(g);
dbg.onEnterFrame = frame => {
    dbg.onEnterFrame = undefined;  // Trigger only once.
    frame.onPop = completion => {
        // Initial yield.
        let genObj = completion.return;
        assertEq(genObj.class, "Generator");
        let result = frame.evalWithBindings("genObj.next()", {genObj});
        assertEq(result.throw.class, "TypeError");
        assertEq(result.throw.getProperty("message").return,
                 "already executing generator");
        hits++;
    };
};

g.f();
assertEq(hits, 1);
