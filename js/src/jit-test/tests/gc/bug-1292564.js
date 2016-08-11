// |jit-test| allow-oom

if (!('oomTest' in this))
    quit();

oomTest(() => {
    let global = newGlobal();
    Debugger(global).onDebuggerStatement = function (frame) {
        frame.eval("f")
    }
    global.eval("debugger")
}, false);
