// Bug 1501666: assertions about the script's step mode count must take
// suspended calls into account. This should not crash.

var g = newGlobal({ newCompartment: true });
g.eval(`
    async function f(y) {
        await true;
        await true;
    };
`);

g.f();
g.f();

var dbg = Debugger(g);
dbg.onEnterFrame = function(frame) {
    frame.onStep = function() {}
}
