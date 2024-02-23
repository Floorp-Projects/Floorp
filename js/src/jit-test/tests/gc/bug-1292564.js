// |jit-test| allow-oom

oomTest(() => {
    let global = newGlobal({sameZoneAs: this});
    Debugger(global).onDebuggerStatement = function (frame) {
        frame.eval("f")
    }
    global.eval("debugger")
}, false);
