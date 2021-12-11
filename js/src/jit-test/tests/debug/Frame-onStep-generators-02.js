// Stepping into the `.throw()` method of a generator with no relevant catch block.
//
// The debugger fires onEnterFrame and then frame.onPop for the generator frame when
// `gen.throw()` is called.

load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});
g.eval(`\
function* z() {         // line 1
    yield 1;            // 2
    yield 2;            // 3
}                       // 4
function f() {          // 5
    let gen = z();      // 6
    gen.next();         // 7
    gen.throw("fit");   // 8
}                       // 9
`);

let log = "";
let previousLine = -1;
let dbg = new Debugger(g);
dbg.onEnterFrame = frame => {
    log += frame.callee.name + "{";
    frame.onStep = () => {
        let line = frame.script.getOffsetLocation(frame.offset).lineNumber;
        if (previousLine != line) { // We stepped to a new line.
            log += line;
            previousLine = line;
        }
    };
    frame.onPop = completion => {
        if ("throw" in completion)
            log += "!";
        log += "}";
    }
};

assertThrowsValue(() => g.f(), "fit");
// z{1} is the initial generator setup.
// z{12} is the first .next() call, running to `yield 1` on line 2
// The final `z{2!}` is for the .throw() call.
assertEq(log, "f{56z{1}67z{12}78z{2!}!}");
