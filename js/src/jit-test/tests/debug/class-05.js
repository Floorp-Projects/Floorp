// |jit-test| error: TypeError

let g = newGlobal();
let dbg = Debugger(g);

let forceException = g.eval(`
    (class extends class {} {
        // Calling this will return a primitive immediately.
        constructor() {
            debugger;
            return {};
        }
    })
`);

let handler = {
    hit() {
        return {
            // Force the return of an illegal value.
            return: 1
        }
    }
};

dbg.onDebuggerStatement = function(frame) {
    var line0 = frame.script.getOffsetLocation(frame.offset).lineNumber;
    var offs = frame.script.getLineOffsets(line0 + 1);
    frame.script.setBreakpoint(offs[0], handler);
}

new forceException;
