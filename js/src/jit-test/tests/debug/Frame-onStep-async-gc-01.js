// An onStep handler on a suspended async function frame keeps a Debugger alive.

let g = newGlobal({newCompartment: true});
g.eval(`
  async function f() {
    debugger;
    await Promise.resolve(0);
    return 'ok';
  }
`);

let dbg = Debugger(g);
let hit = false;
dbg.onDebuggerStatement = frame => {
    frame.onPop = completion => {
        frame.onStep = () => { hit = true; };
        frame.onPop = undefined;
    };
    dbg.onDebuggerStatement = undefined;
    dbg = null;
};

g.f();
assertEq(dbg, null);
gc();
assertEq(hit, false);
drainJobQueue();
assertEq(hit, true);
