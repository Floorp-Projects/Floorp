let g = newGlobal({newCompartment: true});
g.eval('function* f() { yield 123; }');

let dbg = Debugger(g);
let genObj = g.f();

dbg.onEnterFrame = frame => {
    frame.onStep = () => {};
    dbg.removeDebuggee(g);
    dbg.addDebuggee(g);
};

genObj.return();
