// getOffsetLocation agrees with getAllColumnOffsets

var global = newGlobal();
Debugger(global).onDebuggerStatement = function (frame) {
    var script = frame.script;
    var byOffset = [];
    script.getAllColumnOffsets().forEach(function (entry) {
        var {lineNumber, columnNumber, offset} = entry;
        byOffset[offset] = {lineNumber, columnNumber};
    });

    frame.onStep = function() {
        var offset = frame.offset;
        var location = script.getOffsetLocation(offset);
        if (location.isEntryPoint) {
            assertEq(location.lineNumber, byOffset[offset].lineNumber);
            assertEq(location.columnNumber, byOffset[offset].columnNumber);
        } else {
            assertEq(byOffset[offset], undefined);
        }
    };
};

function test(body) {
  print("Test: " + body);
  global.eval(`function f(n) { debugger; ${body} }`);
  global.f(3);
}

test("for (var i = 0; i < n; ++i) ;");
test("var w0,x1=3,y2=4,z3=9");
test("print(n),print(n),print(n)");
test("var o={a:1,b:2,c:3}");
test("var a=[1,2,n]");

global.eval("function ppppp() { return 1; }");
test("1 && ppppp(ppppp()) && new Error()");
