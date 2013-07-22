// eval correctly handles optional custom url option
var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var count = 0;

function testUrl (options, expected) {
    count++;
    dbg.onDebuggerStatement = function (frame) {
        dbg.onNewScript = function (script) {
            dbg.onNewScript = undefined;
            assertEq(script.url, expected);
            count--;
        };
        frame.eval("", options);
    };
    g.eval("debugger;");
}


testUrl(undefined, "debugger eval code");
testUrl(null, "debugger eval code");
testUrl({ url: undefined }, "debugger eval code");
testUrl({ url: null }, "null");
testUrl({ url: 5 }, "5");
testUrl({ url: "test" }, "test");
assertEq(count, 0);
