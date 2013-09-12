// evalInGlobal correctly handles optional lineNumber option
var g = newGlobal();
var dbg = new Debugger(g);
var debuggee = dbg.getDebuggees()[0];
var count = 0;

function testLineNumber (options, expected) {
    count++;
    dbg.onNewScript = function(script){
        dbg.onNewScript = undefined;
        assertEq(script.startLine, expected);
        count--;
    };
    debuggee.evalInGlobal("", options);
}


testLineNumber(undefined, 1);
testLineNumber({}, 1);
testLineNumber({ lineNumber: undefined }, 1);
testLineNumber({ lineNumber: 5 }, 5);
assertEq(count, 0);
