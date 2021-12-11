// onStep works during the evaluation of default parameter values in generators.
//
// (They're evaluated at a weird time in the generator life cycle, before the
// generator object is created.)

load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});
g.eval(`\
    function f1() {}        // line 1
    function f2() {}        //  2
    function f3() {}        //  3
                            //  4
    function* gen(          //  5
        name,               //  6
        schema = f1(),      //  7
        timeToLive = f2(),  //  8
        lucidity = f3()     //  9
    ) {                     // 10
    }                       // 11
`);

let dbg = Debugger(g);
let log = [];
dbg.onEnterFrame = frame => {
    frame.onStep = () => {
        let line = frame.script.getOffsetLocation(frame.offset).lineNumber;
        if (log.length == 0 || line != log[log.length - 1]) {
            log.push(line);
        }
    };
};

g.gen(0);
assertDeepEq(log, [5, 7, 1, 8, 2, 9, 3, 10]);
