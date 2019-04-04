// |jit-test| --enable-experimental-await-fix

// Resolve async function promise when initially awaiting.

let g = newGlobal({newCompartment: true});
let dbg = new Debugger();
let gw = dbg.addDebuggee(g);

g.eval(`
    var resolve;
    var p = new Promise(r => {
        resolve = r;
    });

    async function f() {
        await p;
    }
`);

dbg.onEnterFrame = frame => {
    frame.onPop = completion => {
        g.resolve(0);
        drainJobQueue();
    };
};
g.f();
