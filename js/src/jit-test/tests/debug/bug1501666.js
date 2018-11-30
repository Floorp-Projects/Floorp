async function gg() {
    await 3;
    await 3;
}
gg();
gg();
var g = newGlobal();
g.parent = this;
g.eval(`
    var dbg = Debugger(parent);
    dbg.onEnterFrame = function(frame) {
        frame.onStep = function() {};
    };
`);

drainJobQueue();
