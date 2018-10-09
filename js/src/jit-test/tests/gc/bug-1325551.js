// |jit-test| skip-if: !('oomTest' in this)

let g = newGlobal();
let dbg = new Debugger;
let gw = dbg.addDebuggee(g);
g.eval("function f(){}");
oomTest(() => {
    gw.makeDebuggeeValue(g.f).script.source.sourceMapURL = 'a';
});

