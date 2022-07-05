// Test debugger access to aliased and unaliased bindings work correctly.

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    let env = frame.environment.parent;
    assertEq(env.getVariable('a'), 1);
    assertEq(env.getVariable('b'), 2);
    assertEq(env.getVariable('c'), 3);
    assertEq(env.getVariable('d'), 4);
    assertEq(env.getVariable('e'), 5);
};

g.eval(
`
    let m = parseModule(
    \`
        var a = 1;
        let b = 2;
        export var c = 3;
        export let d = 4;
        let e = 5;
        function f() { debugger; return e; }
    \`);
    moduleLink(m);
    moduleEvaluate(m);
`);

