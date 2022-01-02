// eval correctly handles optional lineNumber option
var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
var count = 0;

function testLineNumber (options, expected) {
    count++;
    dbg.onDebuggerStatement = function (frame) {
        dbg.onNewScript = function (script) {
            dbg.onNewScript = undefined;
            assertEq(script.startLine, expected);
            count--;
        };
        frame.eval("", options);
    };
    g.eval("debugger;");
}


testLineNumber(undefined, 1);
testLineNumber({}, 1);
testLineNumber({ lineNumber: undefined }, 1);
testLineNumber({ lineNumber: 5 }, 5);
assertEq(count, 0);
