// Ensure onPop hook for the final return/yield uses the correct source location
// (closing '}' of the function body).

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
dbg.onEnterFrame = frame => {
    if (frame.type === "global") {
        return;
    }
    frame.onPop = c => {
        if (c.yield !== true) {
            const data = frame.script.getOffsetMetadata(frame.offset);
            g.log.push(`pop(${data.lineNumber}:${data.columnNumber})`);
        }
    };
};
g.evaluate(`                 // line 1
this.log = [];               // 2
function A() {               // 3
    log.push("A");           // 4
    if (log === null) {      // 5
        throw "fail";        // 6
    }                        // 7
}                            // 8
function* B() {              // 9
    log.push("B");           // 10
    if (log === null) {      // 11
        throw "fail";        // 12
    }                        // 13
}                            // 14
async function C() {         // 15
    log.push("C");           // 16
    if (log === null) {      // 17
        throw "fail";        // 18
    }                        // 19
}                            // 20
let D = async () => {        // 21
    log.push("D");           // 22
    if (log === null) {      // 23
        throw "fail";        // 24
    }                        // 25
};                           // 26
class E extends class {} {   // 27
    constructor() {          // 28
        log.push("E");       // 29
        super();             // 30
        if (log === null) {  // 31
            throw "fail";    // 32
        }                    // 33
    }                        // 34
}                            // 35
A();
for (let x of B()) {}
C();
D();
new E();
`);
assertEq(g.log.join(","), "A,pop(8:1),B,pop(14:1),C,pop(20:1),D,pop(26:1),E,pop(27:17),pop(34:5)");
