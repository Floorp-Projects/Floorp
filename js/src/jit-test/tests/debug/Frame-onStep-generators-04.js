// Stepping works across `yield` in generators.

// Set up debuggee.
var g = newGlobal({newCompartment: true});
g.log = "";
g.eval(`
function* range(stop) {               // line 2
    for (let i = 0; i < stop; i++) {  // 3
        yield i;                      // 4
        log += " ";                   // 5
    }                                 // 6
    log += "^";                       // 7
}                                     // 8
`);

// Set up debugger.
let previousLine = -1;
let dbg = new Debugger(g);
dbg.onEnterFrame = frame => {
    frame.onStep = function () {
        assertEq(this, frame);
        let line = frame.script.getOffsetLocation(frame.offset).lineNumber;
        if (previousLine != line) {
            g.log += line; // We stepped to a new line.
            previousLine = line;
        }
    };
    dbg.onEnterFrame = undefined;
};

// Run.
for (let value of g.range(3)) {
    g.log += "*";
}

assertEq(g.log, "234*5 34*5 34*5 37^8");
