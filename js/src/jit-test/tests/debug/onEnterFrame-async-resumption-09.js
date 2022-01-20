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
dbg.onEnterFrame = frame => {
    frame.onPop = completion => {
        if (counter++ === 0) {
            let genObj = completion.return.unsafeDereference();

            genObj.next().then(({value, done}) => {
                assertEq(value, "ponies");
                assertEq(done, true);
            });
        }
    };
};

let it = g.f();

it.next().then(({value, done}) => {
    assertEq(value, undefined);
    assertEq(done, true);
});
