// |jit-test| debug
// We get the same Debug.Script object instance each time we ask.

var global = newGlobal('new-compartment');
global.eval('function f() { debugger; }');
global.eval('function g() { debugger; }');

var debug = new Debug(global);

function evalAndNoteScripts(prog) {
    var scripts = {};
    debug.hooks = {
        debuggerHandler: function(frame) {
            if (frame.type == "call")
                assertEq(frame.script, frame.callee.script);
            scripts.frame = frame.script;
            if (frame.arguments[0])
                scripts.argument = frame.arguments[0].script;
        }
    };
    global.eval(prog);
    return scripts;
}

// If we create a frame for a function and pass it as a value, those should
// both yield the same Debug.Script instance.
var scripts = evalAndNoteScripts('f(f)');
assertEq(scripts.frame, scripts.argument);
var fScript = scripts.argument;

// If we call a second time, we should still get the same instance.
scripts = evalAndNoteScripts('f(f)');
assertEq(scripts.frame, fScript);
assertEq(scripts.argument, fScript);

// If we call with a different argument, we should get a different Debug.Script.
scripts = evalAndNoteScripts('f(g)');
assertEq(scripts.frame !== scripts.argument, true);
assertEq(scripts.frame, fScript);
var gScript = scripts.argument;

// See if we can get g via the frame.
scripts = evalAndNoteScripts('g(f)');
assertEq(scripts.frame !== scripts.argument, true);
assertEq(scripts.frame,    gScript);
assertEq(scripts.argument, fScript);
