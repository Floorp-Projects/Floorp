// |jit-test| debug
// Frame.prototype.script for call frames.

var g = newGlobal();
var dbg = new Debugger(g);

// Apply |f| to each frame that is |skip| frames up from each frame that
// executes a 'debugger' statement when evaluating |code| in the global g.
function ApplyToFrameScript(code, skip, f) {
    dbg.onDebuggerStatement = function (frame) {
        while (skip-- > 0)
            frame = frame.older;
        assertEq(frame.type, "call");
        f(frame.script);
    };
    g.eval(code);
}

ApplyToFrameScript('(function () { debugger; })();', 0,
                   function (script) {
                       assertEq(script instanceof Debugger.Script, true);
                   });

// This would be nice, once we can get host call frames:
// ApplyToFrameScript("(function () { debugger; }).call(null);", 1,
//                    function (script) {
//                        assertEq(script, null);
//                   });
