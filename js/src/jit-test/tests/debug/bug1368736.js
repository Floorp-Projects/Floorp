g = newGlobal({newCompartment: true});
hits = 0;
Debugger(g).onDebuggerStatement = function(frame) {
    // Set a breakpoint at the JSOP_AFTERYIELD op.
    frame.script.setBreakpoint(79, {hit: function() { hits++; }});
}
g.eval(`
function* range() {
    debugger;
    for (var i = 0; i < 3; i++) {
        yield i;
    }
}
var iter = range();
for (var i = 0; i < 3; i++)
    assertEq(iter.next().value, i);
`);
assertEq(hits, 2);
