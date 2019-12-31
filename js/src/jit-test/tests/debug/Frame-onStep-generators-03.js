// Stepping into the `.throw()` method of a generator with a relevant catch block.

load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});
g.eval(`\
function* z() {         // line 1
    try {               // 2
        yield 1;        // 3
    } catch (exc) {     // 4
        yield 2;        // 5
    }                   // 6
}                       // 7
function f() {          // 8
    let gen = z();      // 9
    gen.next();         // 10
    gen.throw("fit");   // 11
}                       // 12
`);

let log = [];
let previousLine = -1;
let dbg = new Debugger(g);
dbg.onEnterFrame = frame => {
    log.push(frame.callee.name + " in");
    frame.onStep = () => {
        let line = frame.script.getOffsetLocation(frame.offset).lineNumber;
        if (previousLine != line) { // We stepped to a new line.
            log.push(line);
            previousLine = line;
        }
    };
    frame.onPop = completion => {
        log.push(frame.callee.name + " out");
    };
};

g.f();
assertEq(
    log.join(", "),
    "f in, 8, 9, z in, 1, z out, " +
    "9, 10, z in, 1, 2, 3, z out, " +
    "10, 11, z in, 3, 2, 4, 5, z out, " +  // not sure why we hit line 2 here, source notes bug maybe
    "11, 12, f out"
);
