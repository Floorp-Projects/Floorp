// Setting onPop handlers from breakpoint handlers works.
var g = newGlobal('new-compartment');
g.eval("function f(){ return 'to normalcy'; }");
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
var log;

// Set a breakpoint at the start of g.f
var gf = gw.makeDebuggeeValue(g.f);
var fStartOffset = gf.script.getLineOffsets(gf.script.startLine)[0];
gf.script.setBreakpoint(fStartOffset, {
    hit: function handleHit(frame) {
        log += 'b';
        frame.onPop = function handlePop(c) {
            log += ')';
            assertEq(c.return, "to normalcy");
        };
    }
});

log = "";
assertEq(g.f(), "to normalcy");
assertEq(log, "b)");
