let g = newGlobal({newCompartment: true});
g.eval('function* f() { yield 123; }');

let dbg = Debugger(g);
dbg.onEnterFrame = frame => {
    dbg.removeDebuggee(g);
    dbg.addDebuggee(g);
};

let genObj = g.f();
genObj.return();
