// Test that block scopes cannot be resurrected by onStep.

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function(frame) {
    frame.onStep = (function() {
        frame.environment;
    });
};

g.eval("debugger; for (let i = 0; i < 1; i++) (function(){});");

// If the last freshened block scope was incorrectly resurrected by onStep
// above, GCing will assert.
gc();

g.eval("debugger; { let i = 0; (function(){ i = 42; }); }");
gc();

g.eval("debugger; try { throw 42; } catch (e) { };");
gc();
