// executeInGlobal correctly handles optional custom url option
var g = newGlobal();
var dbg = new Debugger(g);
var debuggee = dbg.getDebuggees()[0];
var count = 0;

function testUrl (options, expected) {
    count++;
    dbg.onNewScript = function(script){
        dbg.onNewScript = undefined;
        assertEq(script.url, expected);
        count--;
    };
    debuggee.executeInGlobal("", options);
}


testUrl(undefined, "debugger eval code");
testUrl(null, "debugger eval code");
testUrl({ url: undefined }, "debugger eval code");
testUrl({ url: null }, "null");
testUrl({ url: 5 }, "5");
testUrl({ url: "test" }, "test");
assertEq(count, 0);
