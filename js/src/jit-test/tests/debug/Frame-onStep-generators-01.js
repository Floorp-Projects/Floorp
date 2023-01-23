// Stepping into the `.next()` method of a generator works as expected.

let g = newGlobal({newCompartment: true});
g.eval(`\
function* nums() {      // line 1
    yield 1;            //  2
    yield 2;            //  3
}                       //  4
function f() {          //  5
    let gen = nums();   //  6
    gen.next();         //  7
    gen.next();         //  8
    gen.next();         //  9
}                       // 10
`);

let log = [];
let previousLine = -1;
let dbg = new Debugger(g);
dbg.onEnterFrame = frame => {
    frame.onStep = () => {
        let line = frame.script.getOffsetLocation(frame.offset).lineNumber;
        if (previousLine != line) { // We stepped to a new line.
            log.push(line);
            previousLine = line;
        }
    };
};

g.f();
assertEq(log.join(" "), "5 6 1 6 7 1 2 7 8 2 3 8 9 3 4 9 10");
