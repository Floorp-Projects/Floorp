// An onPop handler in a disabled Debugger's frame shouldn't fire.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
g.eval('function f() { debugger; }');
var log;
dbg.onEnterFrame = function handleEnterFrame(f) {
    log += '(';
    assertEq(f.callee.name, 'f');
    f.onPop = function handlePop(c) {
        log += ')';
        assertEq(dbg.enabled, true);
    };
};

var enable;
dbg.onDebuggerStatement = function handleDebugger(f) {
    dbg.enabled = enable;
}


// This should fire the onEnterFrame and onPop handlers.
log = 'a';
enable = true;
g.f();

// This should fire the onEnterFrame handler, but not the onPop.
log += 'b';
enable = false;
g.f();

// This should fire neither.
log += 'c';
dbg.enabled = false;
enable = false;
g.f();

// This should fire both again.
log += 'd';
dbg.enabled = true;
enable = true;
g.f();

assertEq(log, 'a()b(cd()');
