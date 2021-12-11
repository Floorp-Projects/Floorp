function testStepping(script, expected) {
    let g = newGlobal({newCompartment: true});
    let f = g.eval(script);
    let log = [];
    function maybePause(frame) {
        let line = frame.script.getOffsetLocation(frame.offset).lineNumber;
        log.push(line);
    }
    let dbg = new Debugger(g);
    dbg.onEnterFrame = frame => {
        maybePause(frame);
    };
    f();
}
var g7 = newGlobal({newCompartment: true});
g7.parent = this;
g7.eval(`
    Debugger(parent).onEnterFrame = function(frame) {
        let v = frame.environment.getVariable('var0');
    };
`);
testStepping("(function() {})");
gc();
