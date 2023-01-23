// Stepping works across `await` in async functions.

// Set up debuggee.
var g = newGlobal({newCompartment: true});
g.log = "";
g.eval(`                              // line 1
async function aloop() {              // 2
    for (let i = 0; i < 3; i++) {     // 3
        await i;                      // 4
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
g.aloop();
drainJobQueue();

assertEq(g.log, "2345 345 345 37^8");
